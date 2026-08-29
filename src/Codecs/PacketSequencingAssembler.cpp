// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include "PacketSequencingAssembler.h"
#include <Communication/Receiver.h>
#include <Communication/RecoveryFeed.h>
#include <Messages/ValueMessageBuilder.h>
#include <Codecs/Decoder.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;
using namespace Codecs;

namespace
{
  // A linked buffer flag to identify source of buffer
  uint32 FROM_RECOVERY_QUEUE = 1;
}

PacketSequencingAssembler::PacketSequencingAssembler(
      TemplateRegistryPtr templateRegistry,
      HeaderAnalyzer & packetHeaderAnalyzer,
      HeaderAnalyzer & messageHeaderAnalyzer,
      Messages::ValueMessageBuilder & builder,
      size_t lookAheadCount,
      const Communication::RecoveryFeedPtr & recoveryFeed)
  : BasePacketAssembler(
      templateRegistry,
      packetHeaderAnalyzer,
      messageHeaderAnalyzer,
      builder)
  , lookAheadCount_(lookAheadCount)
  , lookAhead_(new Communication::LinkedBuffer *[lookAheadCount])
  , first_(true)
  , nextSequenceNumber_(0)
  , gapWait_(false)
  , gapEnd_(0)
  , recoveryFeed_(recoveryFeed)
  , receiver_(0)
{
  if(!packetHeaderAnalyzer.supportsSequenceNumber())
  {
    throw UsageError("Configuration error", "Arbitrage requires sequence number support from packet header analyzer.");
  }
  // new LinkedBuffer *[0] is legal and yields a valid pointer to no objects,
  // so construction used to succeed and the first line of the service loop
  // then evaluated "% lookAheadCount_" -- a division by zero, and there are
  // nine of them in this file. Zero is a reasonable thing for a caller to
  // pass, meaning "sequence checking without look-ahead buffering", and
  // nothing said otherwise.
  if(lookAheadCount == 0)
  {
    throw UsageError("Configuration error", "Arbitrage requires a look-ahead count of at least one.");
  }
  // The sequence space wraps, so ordering is decided by signed difference.
  // That needs the window to fit in the signed half of the space.
  if(lookAheadCount > size_t(std::numeric_limits<int32>::max()))
  {
    throw UsageError("Configuration error", "Arbitrage look-ahead count is larger than the sequence number space.");
  }
  for(size_t nBuffer = 0; nBuffer < lookAheadCount; ++ nBuffer)
  {
    lookAhead_[nBuffer] = 0;
  }
}

PacketSequencingAssembler::~PacketSequencingAssembler()
{
}

namespace
{
  /// @brief Distance from base to value in a wrapping sequence space.
  ///
  /// sequence_t is a uint32, so a feed wraps every 4.29 billion packets.
  /// Ordinary wraparound survived, because nextSequenceNumber_ wrapped in step
  /// with the feed, but a loss at the boundary did not: with the next expected
  /// number at 0xFFFFFFFE and packet 1 arriving, "1 < 0xFFFFFFFE" is true, so
  /// the packet was released as stale rather than deferred as future. Every
  /// packet after it went the same way and the gap was never reported, because
  /// gap detection depends on packets accumulating in the deferred queue.
  ///
  /// @param value is the sequence number being placed.
  /// @param base is the number it is being placed relative to.
  /// @returns how far ahead of base value is; negative means behind.
  int32 sequenceDistance(sequence_t value, sequence_t base)
  {
    return int32(uint32(value) - uint32(base));
  }
}

bool
PacketSequencingAssembler::serviceQueue(Communication::Receiver & receiver)
{
  bool result = true;
  receiver_ = & receiver;
  bool more = true;
  while(more && result)
  {
    // More becomes true when *something* happens
    more = false;
    /////////////////////////////////////////////////////////////////////
    // Check to see if the next packet is already in the look-ahead array
    Communication::LinkedBuffer * buffer = lookAhead_[nextSequenceNumber_ % lookAheadCount_];
    if(buffer != 0)
    {
      lookAhead_[nextSequenceNumber_ % lookAheadCount_] = 0;
      // decodeBuffer's answer used to be discarded here and in capturePacket,
      // and "result" was assigned once at its declaration and returned
      // unchanged. So setMessageLimit had no effect -- the limit was computed,
      // returned and dropped -- and a builder that answered reportDecodingError
      // with false, its way of saying the stream is unusable, was ignored.
      result = processPacket(buffer);
      more = true;
    }

    if(!more)
    {
      ////////////////////////////////
      // Handle a buffer from receiver
      // Note the receiver merges the
      // A/B feeds into a single queue
      buffer = receiver.getBuffer(false);
      if(buffer != 0)
      {
        buffer->clearFlag(FROM_RECOVERY_QUEUE);
        result = capturePacket(buffer);
        more = true;
      }
    }

    if(!more && recoveryFeed_)
    {
      //////////////////////////////////////
      // Handle a buffer from recovery feed

      // recoveryIncoming_ is a local queue that does
      // not need thread synchronization (serviceQueue is protected)
      // It is filled with a single call to minimize mutex locking.
      if(recoveryIncoming_.isEmpty())
      {
        // pull all available packets into recoveryIncoming_
        recoveryFeed_->fetchBuffers(recoveryIncoming_);
      }
      buffer = recoveryIncoming_.pop();
      if(buffer != 0)
      {
        buffer->setFlag(FROM_RECOVERY_QUEUE);
        result = capturePacket(buffer);
        more = true;
      }
    }
    if(!more)
    {
      ////////////////////////////////////////
      // The next buffer is not available(yet)
      // If any deferred buffers are now within the
      // lookahead range, promote them to the lookahead array.
      more = promoteDeferred();
    }
    if(!more && (sequenceDistance(nextSequenceNumber_, gapEnd_) < 0 || !deferredQueue_.isEmpty()))
    {
      /////////////////////////////////////////////////////////////////////////////////////////////
      // if the next sequence number is < end of the gap we are filling a previously discovered gap
      // if the deferred queue is not empty after any any available packets have been processed, we
      // have a new gap.
      // In either case, handle it.
      // Note this may wait until messages arrive on the recovery feed.
      handleGap();
      // and continue trying to process the next buffer
      more = true;
    }
  }
  // if we're completely up-to-date, return from serviceQueue
  receiver_ = 0;
  return result;
}


bool
PacketSequencingAssembler::capturePacket(Communication::LinkedBuffer * buffer)
{
  sequence_t sequenceNumber = 0;
  try
  {
    sequenceNumber = packetHeaderAnalyzer_.getSequenceNumber(buffer->get(), buffer->used());
  }
  catch(const std::exception & ex)
  {
    // The sequence number used to be read without reference to buffer->used(),
    // so a datagram shorter than the configured offset and length -- a
    // keepalive, a probe, a truncated packet, all of which come from the
    // network -- assembled its number from stale bytes left in the pooled
    // buffer by a previous packet. That arbitrary number then drove the
    // routing below: silently discarded as old, filed in the wrong look-ahead
    // slot, or deferred as far-future, where it could fabricate a gap and
    // trigger a retransmission request for packets that were never missing.
    const bool more = builder_.reportDecodingError(ex.what());
    releasePacket(buffer);
    return more;
  }
  if(first_)
  {
    first_ = false;
    nextSequenceNumber_ = sequenceNumber;
  }
  const int32 distance = sequenceDistance(sequenceNumber, nextSequenceNumber_);
  if(distance == 0)
  {
    return processPacket(buffer);
  }
  else if(distance < 0)
  {
    releasePacket(buffer);
  }
  else if(distance < int32(lookAheadCount_))
  {
    if(lookAhead_[sequenceNumber % lookAheadCount_] != 0)
    {
      releasePacket(buffer);
    }
    else
    {
      lookAhead_[sequenceNumber % lookAheadCount_] = buffer;
    }
  }
  else
  {
    /// buffer is beyond look-ahead
    addToDeferred(buffer, sequenceNumber);
  }
  return true;
}

bool
PacketSequencingAssembler::processPacket(Communication::LinkedBuffer * buffer)
{
  const bool more = decodeBuffer(buffer->get(), buffer->used());
  releasePacket(buffer);
  ++nextSequenceNumber_;
  return more;
}

void
PacketSequencingAssembler::releasePacket(Communication::LinkedBuffer * buffer)
{
  if(buffer->checkAnyFlag(FROM_RECOVERY_QUEUE) && recoveryFeed_)
  {
    recoveryFeed_->releaseBuffer(buffer);
  }
  else
  {
    receiver_->releaseBuffer(buffer);
  }
}

void
PacketSequencingAssembler::addToDeferred(Communication::LinkedBuffer * buffer, sequence_t sequenceNumber)
{
  // check for empty deferred queue
  Communication::LinkedBuffer * positionInDeferred = deferredQueue_.peek();
  if(positionInDeferred == 0)
  {
    deferredQueue_.push_front(buffer);
    return;
  }

  /// because it's likely that this goes at the end of the queue, check that before walking the queue.
  Communication::LinkedBuffer * tail = deferredQueue_.peek_tail();
  if(tail == 0)
  {
    deferredQueue_.push_front(buffer);
    return;
  }
  sequence_t deferredSequenceNumber = packetHeaderAnalyzer_.getSequenceNumber(tail->get(), tail->used());
  if(sequenceNumber == deferredSequenceNumber)
  {
    releasePacket(buffer);
    return;
  }
  if(sequenceDistance(sequenceNumber, deferredSequenceNumber) > 0)
  {
    deferredQueue_.push(buffer);
    return;
  }

  // the other "easy" case is the buffer comes before everything in the deferred queue
  deferredSequenceNumber = packetHeaderAnalyzer_.getSequenceNumber(positionInDeferred->get(), positionInDeferred->used());
  if(sequenceDistance(sequenceNumber, deferredSequenceNumber) < 0)
  {
    deferredQueue_.push_front(buffer);
    return;
  }
  if(sequenceNumber == deferredSequenceNumber)
  {
    releasePacket(buffer);
    return;
  }

  // the new packet doesn't belong at either end.
  // we have to walk the queue to insert the packet in-place
  // This requires too-much-knowledge of the BufferQueue, but the alternatives are worse.
  //
  // loop invariant: sequenceNumber > deferredSequenceNumber
  while(positionInDeferred->link() != 0)
  {
    deferredSequenceNumber = packetHeaderAnalyzer_.getSequenceNumber(positionInDeferred->link()->get(), positionInDeferred->link()->used());
    if(sequenceNumber == deferredSequenceNumber)
    {
      releasePacket(buffer);
      return;
    }
    if(sequenceDistance(sequenceNumber, deferredSequenceNumber) < 0)
    {
      buffer->link(positionInDeferred->link());
      positionInDeferred->link(buffer);
      return;
    }
    positionInDeferred = positionInDeferred->link();
  }
  // reached the end of the queue
  // THIS SHOULD NOT HAPPEN 'cause we checked this case above
  deferredQueue_.push(buffer);
}

bool
PacketSequencingAssembler::promoteDeferred()
{
  bool result = false;
  Communication::LinkedBuffer * buffer = deferredQueue_.peek();
  while(buffer != 0
    && sequenceDistance(
        packetHeaderAnalyzer_.getSequenceNumber(buffer->get(), buffer->used()),
        nextSequenceNumber_) < 0)
  {
    releasePacket(deferredQueue_.pop());
    buffer = deferredQueue_.peek();
  }

  while(buffer != 0
    && sequenceDistance(
        packetHeaderAnalyzer_.getSequenceNumber(buffer->get(), buffer->used()),
        nextSequenceNumber_) < int32(lookAheadCount_))
  {
    buffer = deferredQueue_.pop();
    sequence_t sequenceNumber = packetHeaderAnalyzer_.getSequenceNumber(buffer->get(), buffer->used());
    if(lookAhead_[sequenceNumber % lookAheadCount_] == 0)
    {
      lookAhead_[sequenceNumber % lookAheadCount_] = buffer;
      result = true;
    }
    else
    {
      releasePacket(buffer);
    }
    buffer = deferredQueue_.peek();
  }
  return result;
}

void
PacketSequencingAssembler::handleGap()
{
  // If this is a new gap
  if(sequenceDistance(nextSequenceNumber_, gapEnd_) >= 0)
  {
    gapEnd_ = findGapEnd();
    gapWait_ = false;
    if(recoveryFeed_)
    {
      // Initiate the process of filling the gap.
      gapWait_ = recoveryFeed_->reportGap(nextSequenceNumber_, gapEnd_);
    }
  }
  else
  {
    sequence_t newGapEnd = findGapEnd();
    if(sequenceDistance(newGapEnd, gapEnd_) < 0)
    {
      gapEnd_ = newGapEnd;
    }
    if(recoveryFeed_)
    {
      // this gives the recovery feed a chance to:
      //    retry the refill request if it has taken too long, or
      //    reduce the number of packets needed if the gap has gotten smaller, or
      //    say "never mind" this gap will never be filled.
      // The new gap will always be completely contained within the previous gap, so
      // the recovery feed can ignore this call if it is of a mind to.
      gapWait_ = recoveryFeed_->stillWaiting(nextSequenceNumber_, gapEnd_);
    }
  }

  if(!gapWait_)
  {
    builder_.reportGap(nextSequenceNumber_, gapEnd_);
    nextSequenceNumber_ = gapEnd_;
  }
  else if(recoveryFeed_)
  {
    // We're waiting for the recovery feed to fill the gap
    // delay until the recovery feed has data (or times out)
    // The timeout is there in the unlikely event that the missing packet(s)
    // magically arrive(s) on one of the primary (A/B) feeds.
    recoveryFeed_->waitGapFill(std::chrono::milliseconds(10));
  }
}

sequence_t
PacketSequencingAssembler::findGapEnd() const
{
  sequence_t gapEnd = nextSequenceNumber_ + 1;
  while(sequenceDistance(gapEnd, nextSequenceNumber_) < int32(lookAheadCount_))
  {
    if(lookAhead_[gapEnd % lookAheadCount_] != 0)
    {
      return gapEnd;
    }
    ++gapEnd;
  }
  Communication::LinkedBuffer * deferredBuffer = deferredQueue_.peek();
  if(deferredBuffer == 0)
  {
    // The comment here used to state this precondition and leave it at that.
    // Nothing in the code as written can reach it -- the invariant holds, but
    // it is distributed across handleGap's branch structure, its caller's
    // guard, and promoteDeferred's release conditions, so every future reader
    // has to re-derive it across three functions to be sure this pointer is
    // not null. An empty window and an empty deferred queue mean the gap is
    // exactly one packet wide.
    return nextSequenceNumber_ + 1;
  }
  return packetHeaderAnalyzer_.getSequenceNumber(deferredBuffer->get(), deferredBuffer->used());
}
