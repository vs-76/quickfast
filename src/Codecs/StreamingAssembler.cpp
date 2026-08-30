// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include "StreamingAssembler.h"
#include <Communication/Receiver.h>
#include <Messages/ValueMessageBuilder.h>
#include <Codecs/DataSourceBuffer.h>
#include <Codecs/Decoder.h>

using namespace QuickFAST;
using namespace Codecs;

StreamingAssembler::StreamingAssembler(
      TemplateRegistryPtr templateRegistry,
      HeaderAnalyzer & headerAnalyzer,
      Messages::ValueMessageBuilder & builder,
      bool waitForCompleteMessage)
  : Communication::Assembler(templateRegistry, builder)
  , headerAnalyzer_(headerAnalyzer)
  , builder_(builder)
  , stopping_(false)
  , waitForCompleteMessage_(waitForCompleteMessage)
  , receiver_(0)
  , currentBuffer_(0)
  , headerIsComplete_(false)
  , skipBlock_(false)
  , blockSize_(0)
  , inDecoder_(false)
  , messageLimit_(0)
{
}

StreamingAssembler::~StreamingAssembler()
{
}

void
StreamingAssembler::reportHeaderFailure(
  Communication::Receiver & receiver,
  const std::string & what)
{
  builder_.reportDecodingError(what);
  // A message that fails to decode still leaves the next message boundary
  // known, so the decode path can be told to carry on. A header that fails
  // does not: the framing is what was lost, and there is no position in the
  // stream left to resume from. Stopping is the only honest answer.
  stopping_ = true;
  headerIsComplete_ = false;
  blockSize_ = 0;
  skipBlock_ = false;
  if(currentBuffer_ != 0)
  {
    receiver.releaseBuffer(currentBuffer_);
    currentBuffer_ = 0;
  }
}

bool
StreamingAssembler::serviceQueue(
  Communication::Receiver & receiver)
{
  // save the receiver so callbacks from the decoder can find it.
  receiver_ = &receiver;
  bool more = true;
  while(more && !stopping_)
  {
    if(!headerIsComplete_)
    {
      // analyzeHeader was outside every error path, so a header analyzer that
      // rejected its input took the whole receiver down instead of being
      // reported as the decoding error it is. BasePacketAssembler has always
      // wrapped its two calls; this one was the exception.
      try
      {
        headerIsComplete_ = headerAnalyzer_.analyzeHeader(*this, blockSize_, skipBlock_);
      }
      catch(std::exception & ex)
      {
        reportHeaderFailure(receiver, ex.what());
        break;
      }
    }
    more = headerIsComplete_;

    if(more)
    {
      if(waitForCompleteMessage_ && blockSize_ > 0)
      {
        // A block larger than the whole buffer pool can never be assembled,
        // so waiting for it is not patience but a permanent stall: the header
        // stays consumed, headerIsComplete_ stays set, and every later packet
        // is counted towards a block that will never be complete. The stream
        // is dead from here on, and saying so beats hanging silently.
        const size_t capacity = receiver_->totalBufferCapacity();
        if(capacity != 0 && blockSize_ > capacity)
        {
          std::stringstream message;
          message << "Block size of " << blockSize_
            << " bytes exceeds the " << capacity
            << " bytes the receiver can hold. The stream cannot be assembled.";
          reportHeaderFailure(receiver, message.str());
          break;
        }

        /// check # bytes available to see if there's a complete message to decode
        size_t available = currentBytesAvailable();

        // try for more bytes: false means don't wait if they aren't there
        if(available < blockSize_
          && !receiver_->needBytes(blockSize_ - available, false))
        {
          // need more data;  Return and continue receiving
          more = false;
        }
      }
    }
    if(more)
    {
      headerIsComplete_ = 0;
      blockSize_ = 0;
      if(skipBlock_)
      {
//not implemented yet        builder_.skipMessage();
      }
      skipBlock_ = false;
      if(messageAvailable() > 0)
      {
        // Set this to indicate we block during decoding
        inDecoder_ = true;
        try
        {
          if(reset_)
          {
            decoder_.reset();
          }
          // Prefer the buffer that currently holds the message start; messages
          // that later span buffers keep this first arrival time.
          if(currentBuffer_ != 0 && currentBuffer_->hasReceiveTime())
          {
            builder_.setReceiveTime(currentBuffer_->receiveTime());
          }
          else
          {
            builder_.clearReceiveTime();
          }
          decoder_.decodeMessage(*this, builder_);
        }
        catch(std::exception & ex)
        {
          more = builder_.reportDecodingError(ex.what());
          if(!more)
          {
            stopping_ = true;
            if(currentBuffer_ != 0)
            {
              receiver.releaseBuffer(currentBuffer_);
              currentBuffer_ = 0;
            }
          }
        }
        inDecoder_ = false;
      }
      else
      {
        more = false;
      }
    }
  }
  receiver_ = 0;
  return !stopping_;
}

void
StreamingAssembler::receiverStarted(Communication::Receiver & /*receiver*/)
{
  decoder_.setStrict(strict_);
  if(builder_.wantLog(Common::Logger::QF_LOG_INFO))
  {
    builder_.logMessage(Common::Logger::QF_LOG_INFO, "Start receiver.");
  }
}

void
StreamingAssembler::receiverStopped(Communication::Receiver & receiver)
{
  if(builder_.wantLog(Common::Logger::QF_LOG_INFO))
  {
    builder_.logMessage(Common::Logger::QF_LOG_INFO, "Stop receiver.");
  }

  stopping_ = true;
  if(currentBuffer_ != 0)
  {
    receiver.releaseBuffer(currentBuffer_);
    currentBuffer_ = 0;
  }
}


bool
StreamingAssembler::getBuffer(const uchar *& buffer, size_t & size)
{
  size = 0;
  if(currentBuffer_ != 0)
  {
    if(receiver_ == 0)
    {
      throw UsageError(
        "Internal Error",
        "StreamingAssembler::readByte called in the wrong scope.");
    }
    receiver_->releaseBuffer(currentBuffer_);
    currentBuffer_ = 0;
  }

  // Look for a new buffer.  If we're in the decoder, wait for it.
  if(inDecoder_)
  {
    receiver_->waitBuffer();
  }
  currentBuffer_ = receiver_->getBuffer(inDecoder_);
  if(currentBuffer_ != 0)
  {
    buffer = currentBuffer_->get();
    size = currentBuffer_->used();
  }
  return size > 0;
}

int
StreamingAssembler::messageAvailable()
{
  if(stopping_)
  {
    return -1;
  }
  return bytesAvailable();
}

