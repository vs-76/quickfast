// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include <Communication/Receiver.h>
#include "BasePacketAssembler.h"
#include <Messages/ValueMessageBuilder.h>
#include <Codecs/Decoder.h>

using namespace QuickFAST;
using namespace Codecs;

BasePacketAssembler::BasePacketAssembler(
      TemplateRegistryPtr templateRegistry,
      HeaderAnalyzer & packetHeaderAnalyzer,
      HeaderAnalyzer & messageHeaderAnalyzer,
      Messages::ValueMessageBuilder & builder)
  : Communication::Assembler(templateRegistry, builder)
  , packetHeaderAnalyzer_(packetHeaderAnalyzer)
  , messageHeaderAnalyzer_(messageHeaderAnalyzer)
  , builder_(builder)
  , messageCount_(0)
  , receivedByteCount_(0)
  , messageLimit_(0)
{
}

BasePacketAssembler::~BasePacketAssembler()
{
}

bool
BasePacketAssembler::decodeBuffer(const unsigned char * buffer, size_t size)
{
  bool result = true;
  ++messageCount_;
  // Was "++receivedByteCount_ += size", which adds one byte per packet to the
  // figure throughput reporting is computed from.
  receivedByteCount_ += size;
  if(builder_.wantLog(Common::Logger::QF_LOG_VERBOSE))
  {
    std::stringstream message;
    message << "Received[" << messageCount_ << "]: " << size << " bytes" << std::endl;
    for(size_t i = 0; i < size; ++i)
    {
      message << std::hex << std::setw(2) << std::setfill('0') << (unsigned short)(buffer[i]) << std::setfill(' ') << std::dec << ' ';
      if((i+1)% 32 == 0)
      {
        message << std::endl;
      }
    }
    message << std::endl;
    builder_.logMessage(Common::Logger::QF_LOG_VERBOSE, message.str());
  }
  try
  {
    currentBuffer_ = buffer;
    currentSize_ = size;
    size_t blockSize = 0;
    bool skipBlock = false;
    if(!packetHeaderAnalyzer_.analyzeHeader(*this, blockSize, skipBlock))
    {
      // header must be complete in one packet
      builder_.reportDecodingError("Invalid header in packet.  Ignoring packet.");
      packetHeaderAnalyzer_.reset();
      DataSource::reset();
      currentSize_ = 0;
      currentBuffer_ = 0;
    }
    else if(bytesAvailable() < 0 || blockSize > size_t(bytesAvailable()))
    {
      // The declared length used to be parsed and then never read again, here
      // and for messageSize below, so a block claiming more bytes than the
      // packet carries was not detected at the framing layer at all: the
      // decoders were left to discover it by hitting EOF part way through a
      // value. That is also why findings #38 to #40 went unnoticed, since in
      // this path the block size was computed wrongly and then discarded.
      std::stringstream message;
      message << "Packet declares a block of " << blockSize
        << " bytes but carries " << bytesAvailable()
        << ". Ignoring packet.";
      builder_.reportDecodingError(message.str());
      packetHeaderAnalyzer_.reset();
      DataSource::reset();
      currentSize_ = 0;
      currentBuffer_ = 0;
    }
    else
    {
      if(skipBlock)
      {
        DataSource::reset();
        currentSize_ = 0;
        currentBuffer_ = 0;
      }
      else
      {
        // note we apply reset at the packet level. If there are multiple messages per packet
        // the decoder is NOT reset for each one.
        if(reset_)
        {
          decoder_.reset();
        }
        while(bytesAvailable() > 0)
        {
          bool skipMessage = false;
          size_t messageSize = 0;
          if(!messageHeaderAnalyzer_.analyzeHeader(*this, messageSize, skipMessage))
          {
            // header must be complete in one packet
            builder_.reportDecodingError("Invalid message header.  Ignoring remainder of packet.");
            messageHeaderAnalyzer_.reset();
            DataSource::reset();
            currentSize_ = 0;
            currentBuffer_ = 0;
          }
          else if(bytesAvailable() < 0 || messageSize > size_t(bytesAvailable()))
          {
            std::stringstream message;
            message << "Message declares " << messageSize
              << " bytes but " << bytesAvailable()
              << " remain in the packet. Ignoring remainder of packet.";
            builder_.reportDecodingError(message.str());
            messageHeaderAnalyzer_.reset();
            DataSource::reset();
            currentSize_ = 0;
            currentBuffer_ = 0;
          }
          else if(skipMessage)
          {
            DataSource::reset();
            currentSize_ = 0;
            currentBuffer_ = 0;
          }
          else
          {
            decoder_.decodeMessage(*this, builder_);
          }
        }
      }
    }
  }
  catch (const std::exception &ex)
  {
    result = builder_.reportDecodingError(ex.what());
    reset();
  }
  if(result && messageCount_ > messageLimit_ && messageLimit_ != 0)
  {
    result = false;
  }
  // an aid to debugging
  // memset(const_cast<unsigned char *>(buffer), '\xCD' , size);
  return result;
}

void
BasePacketAssembler::receiverStarted(Communication::Receiver & /*receiver*/)
{
  decoder_.setStrict(strict_);
  if(builder_.wantLog(Common::Logger::QF_LOG_INFO))
  {
    builder_.logMessage(Common::Logger::QF_LOG_INFO, "Receiver started");
  }
}
void
BasePacketAssembler::receiverStopped(Communication::Receiver & /*receiver*/)
{
  if(builder_.wantLog(Common::Logger::QF_LOG_INFO))
  {
    builder_.logMessage(Common::Logger::QF_LOG_INFO, "Receiver stopped");
  }
}

bool
BasePacketAssembler::getBuffer(const uchar *& buffer, size_t & size)
{
  bool result = currentSize_ > 0;
  buffer = currentBuffer_;
  currentBuffer_ = 0;
  size = currentSize_;
  currentSize_ = 0;
  return result;
}

