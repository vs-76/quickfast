// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FixedSizeHeaderAnalyzer.h"
#include <Common/Types.h>
#include <Codecs/DataSource.h>
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Codecs;

FixedSizeHeaderAnalyzer::FixedSizeHeaderAnalyzer(
  size_t sizeBytes,
  bool bigEndian,
  size_t prefixBytes,
  size_t suffixBytes,
  size_t sequenceOffset,
  size_t sequenceLength
  )
: prefixBytes_(prefixBytes)
, sizeBytes_(sizeBytes)
, suffixBytes_(suffixBytes)
, sequenceOffset_(sequenceOffset)
, sequenceLength_(sequenceLength)
, bigEndian_(bigEndian)
  // The block size is assembled arithmetically from bytes read one at a time,
  // and shifts and ORs are defined on values rather than on memory layout, so
  // the host's byte order is irrelevant here. Consulting
  // ByteSwapper::isBigEndian() as well inverted the choice on every
  // little-endian host -- which in practice is every host -- so both
  // configurations read the size backwards. getSequenceNumber, two functions
  // below, tests bigEndian_ directly and has always been right.
, swapNeeded_(!bigEndian)
, state_(ParsingIdle)
, blockSize_(0)
, byteCount_(0)
, testSkip_(0)
, headersParsed_(0)
{
  if(sizeBytes > sizeof(size_t))
  {
    std::stringstream msg;
    msg << "Fixed size header size field of " << sizeBytes
        << " bytes exceeds the " << sizeof(size_t)
        << " bytes a block size can hold.";
    throw UsageError("Invalid configuration", msg.str().c_str());
  }
  // A sequence field wider than the accumulator shifted its own leading bytes
  // off the top, so the number reported bore no relation to the one on the
  // wire and nothing said so.
  if(sequenceLength > sizeof(uint32))
  {
    std::stringstream msg;
    msg << "Fixed size header sequence number field of " << sequenceLength
        << " bytes exceeds the " << sizeof(uint32)
        << " bytes a sequence number can hold.";
    throw UsageError("Invalid configuration", msg.str().c_str());
  }
}

FixedSizeHeaderAnalyzer::~FixedSizeHeaderAnalyzer()
{
}

bool
FixedSizeHeaderAnalyzer::analyzeHeader(DataSource & source, size_t & blockSize, bool & skip)
{
  skip = false;
  blockSize = 0;
  while(state_ != ParsingComplete)
  {
    switch(state_)
    {
    case ParsingIdle:
      {
        source.beginField("FIXED_SIZE_HEADER");
        state_ = ParsingPrefix;
        byteCount_ = 0;
        break;
      }
    case ParsingPrefix:
      {
        while(byteCount_ < prefixBytes_)
        {
          uchar next = 0;
          if(!source.getByte(next))
          {
            return false;
          }
          ++byteCount_;
        }
        state_ = ParsingBlockSize;
        byteCount_ = 0;
        blockSize_ = 0;
        break;
      }
    case ParsingBlockSize:
      {
        while(byteCount_ < sizeBytes_)
        {
          uchar next = 0;
          if(!source.getByte(next))
          {
            return false;
          }
          if(swapNeeded_)
          {
            // next & 0xFF promotes to int, so this shifted an int: a most
            // significant byte with the high bit set produced a negative
            // value that then sign-extended across the top of the size_t,
            // turning a four gigabyte size into eighteen exabytes, and any
            // size field wider than four bytes shifted by 32 or more, which
            // is undefined rather than a wrap.
            blockSize_ |= size_t(next) << (byteCount_ * 8);
          }
          else
          {
            blockSize_ <<= 8;
            blockSize_ |= (next & 0xFF);
          }
          ++byteCount_;
        }
        state_ = ParsingSuffix;
        byteCount_ = 0;

        break;
      }
    case ParsingSuffix:
      {
        while(byteCount_ < suffixBytes_)
        {
          uchar next = 0;
          if(!source.getByte(next))
          {
            return false;
          }
          ++byteCount_;
        }
        state_ = ParsingComplete;
        break;
      }
    default:
      {
        break;
      }
    }
  }
  state_ = ParsingIdle;
  blockSize = blockSize_;
  blockSize_ = 0;
  byteCount_ = 0;
  if(testSkip_ != 0 && (++headersParsed_ % testSkip_ == 0))
  {
    std::cout << std::endl << "SKIPPING HEADER " << headersParsed_ << std::endl;
    skip = true;
  }
  return true;
}

bool
FixedSizeHeaderAnalyzer::supportsSequenceNumber()const
{
  return true;
}

uint32
FixedSizeHeaderAnalyzer::getSequenceNumber(const uchar * buffer, size_t size) const
{
  // Without a length this read was unbounded: the offset and length come from
  // constructor arguments that were validated against nothing, and the caller
  // had no way to know how far the function would reach.
  if(sequenceOffset_ + sequenceLength_ > size)
  {
    std::stringstream msg;
    msg << "Sequence number at offset " << sequenceOffset_
        << " for " << sequenceLength_
        << " bytes runs past the end of a " << size << " byte header.";
    throw UsageError("Invalid header", msg.str().c_str());
  }

  uint32 value = 0;
  if(bigEndian_)
  {
    size_t nByte = 0;
    while(nByte < sequenceLength_)
    {
      value <<= 8;
      value |= buffer[sequenceOffset_ + nByte++];
    }
  }
  else
  {
    size_t nByte = sequenceLength_;
    while(nByte != 0)
    {
      value <<= 8;
      value |= buffer[sequenceOffset_ + --nByte];
    }
  }
  return value;
}


void
FixedSizeHeaderAnalyzer::reset()
{
  state_ = ParsingIdle;
  blockSize_ = 0;
  byteCount_ = 0;
}
