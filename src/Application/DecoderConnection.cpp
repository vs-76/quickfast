// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include "DecoderConnection.h"
#include <Application/DecoderConfiguration_fwd.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/MessagePerPacketAssembler.h>
#include <Codecs/StreamingAssembler.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>
#include <Codecs/FastEncodedHeaderAnalyzer.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/DataSource.h>

#include <Communication/MulticastReceiver.h>
#include <Communication/SourceSpecificMulticastReceiver.h>
#include <Communication/TCPReceiver.h>
#include <Communication/RawFileReceiver.h>
#include <Communication/BufferedRawFileReceiver.h>
#include <Communication/PCapFileReceiver.h>
#include <Communication/AsynchFileReceiver.h>
#include <Communication/BufferReceiver.h>
#include <Communication/AsioService.h>

using namespace QuickFAST;
using namespace Application;

namespace
{
#ifdef _WIN32
  const std::ios::openmode binaryMode = std::ios::binary;
#else
  const std::ios::openmode binaryMode = static_cast<std::ios::openmode>(0);
#endif
}


DecoderConnection::DecoderConnection()
{
}

DecoderConnection::~DecoderConnection()
{
}

void
DecoderConnection::setTemplateRegistry(Codecs::TemplateRegistryPtr registry)
{
  registry_ = registry;
}

void
DecoderConnection::createSourceSpecificMulticastReceiver(
  const Application::DecoderConfiguration & configuration)
{
  // One receiver could carry feeds of both kinds, but one configuration
  // should not: -msource applies to the feed named by the most recent
  // -mname, so a misplaced option leaves a feed silently any-source --
  // accepting exactly the senders the operator asked to exclude.
  for(size_t nFeed = 0; nFeed < configuration.multicastCount(); ++nFeed)
  {
    if(configuration.multicastSourceIPs(nFeed).empty())
    {
      std::stringstream msg;
      msg << "Multicast feed " << configuration.multicastName(nFeed)
        << " names no source while other feeds in this connection do."
        << "  Use -msource for every feed, or for none.";
      throw std::invalid_argument(msg.str());
    }
  }

  Communication::SourceSpecificMulticastReceiver * receiver;
  if(configuration.privateIOService())
  {
    ioService_.reset(new asio::io_context);
    receiver = new Communication::SourceSpecificMulticastReceiver(*ioService_);
  }
  else
  {
    receiver = new Communication::SourceSpecificMulticastReceiver();
  }
  receiver_.reset(receiver);

  for(size_t nFeed = 0; nFeed < configuration.multicastCount(); ++nFeed)
  {
    // An unset bind address is passed on as unset: the receiver binds a
    // source specific feed to the group, not to the listen interface.
    receiver->addFeed(
      configuration.multicastName(nFeed),
      configuration.multicastGroupIP(nFeed),
      configuration.multicastSourceIPs(nFeed),
      configuration.listenInterfaceIP(nFeed),
      configuration.multicastBindIPIsSet(nFeed)
        ? configuration.multicastBindIP(nFeed)
        : std::string(),
      configuration.portNumber(nFeed)
      );
  }
}

void
DecoderConnection::configure(
  Messages::ValueMessageBuilder & builder,
  Application::DecoderConfiguration &configuration)
{
  // DecoderConfiguration documents, three times, that
  // bufferCount * bufferSize must equal or exceed the maximum message size.
  // Zero times anything is zero, so a zero here violates the stated
  // precondition. Nothing checked it: the receiver reported a successful
  // start and the event loop then waited forever for a read that a pool of
  // no bytes could never satisfy. A hang is the worst answer to a typo,
  // because it looks like progress.
  if(configuration.bufferSize() == 0)
  {
    throw UsageError(
      "Invalid configuration",
      "Buffer size must be greater than zero.");
  }
  if(configuration.bufferCount() == 0)
  {
    throw UsageError(
      "Invalid configuration",
      "Buffer count must be greater than zero.");
  }

  if(!configuration.asynchReads() && !configuration.fastFileName().empty())
  {
#ifndef _WIN32
    // on Windows cin is opened in ascii mode which garbles FAST data
    if(configuration.fastFileName() == "cin")
    {
      fastFile_ = &std::cin;
    }
    else
#endif
    {
      ownedFastFile_ = std::make_unique<std::ifstream>(
        configuration.fastFileName().c_str(), binaryMode);
      fastFile_ = ownedFastFile_.get();
    }
    if(!fastFile_->good())
    {
      std::stringstream msg;
      msg << "Can't open FAST data file: "
        << configuration.fastFileName();
      throw std::invalid_argument(msg.str());
    }
  }

  if(!configuration.verboseFileName().empty())
  {
    if(configuration.verboseFileName() == "cerr")
    {
      verboseFile_= &std::cerr;
    }
    else if(configuration.verboseFileName() == "cout")
    {
      verboseFile_ = &std::cout;
    }
    else
    {
      ownedVerboseFile_ = std::make_unique<std::ofstream>(
        configuration.verboseFileName().c_str());
      verboseFile_ = ownedVerboseFile_.get();
      if(!verboseFile_->good())
      {
        std::stringstream msg;
        msg << "Can't open verbose file: "
          << configuration.verboseFileName();
        throw std::invalid_argument(msg.str());
      }
    }
  }

  if(! configuration.echoFileName().empty())
  {
    std::ios::openmode mode = std::ios::out | std::ios::trunc;
    if(configuration.echoType() == DecoderConfigurationEnums::RAW)
    {
      mode |= binaryMode;
    }
    if(configuration.echoFileName() == "cout")
    {
      echoFile_ = & std::cout;
    }
    else if(configuration.echoFileName() == "cerr")
    {
      echoFile_ = & std::cerr;
    }
    else
    {
      ownedEchoFile_ = std::make_unique<std::ofstream>(
        configuration.echoFileName().c_str(), mode);
      echoFile_ = ownedEchoFile_.get();
    }
    if(!echoFile_->good())
    {
      std::stringstream msg;
      msg << "Can't open echo file: "
        << configuration.echoFileName();
      throw std::invalid_argument(msg.str());
    }
  }

  if(!registry_)
  {
    std::ifstream templates(configuration.templateFileName().c_str(),
      std::ios::in | binaryMode
      );
    if(!templates.good())
    {
        std::stringstream msg;
        msg << "Can't open template file: "
          << configuration.templateFileName();
        throw std::invalid_argument(msg.str());
    }
    Codecs::XMLTemplateParser parser;
    // Same shape as the echo stream: verboseFile_ stays null without -vo, and
    // forming a reference from it is undefined even though the parser stores
    // the address and treats null as "not verbose".
    if(verboseFile_ != nullptr)
    {
      parser.setVerboseOutput(*verboseFile_);
    }
    parser.setNonstandard(configuration.nonstandard());

    registry_ = parser.parse(templates);
  }

  if((configuration.nonstandard() & 4) != 0)
  {
    registry_->display(std::cout, 0);
  }

  switch(configuration.packetHeaderType())
  {
  case Application::DecoderConfiguration::NO_HEADER:
    {
      Codecs::NoHeaderAnalyzer * analyzer = new Codecs::NoHeaderAnalyzer;
      analyzer->setTestSkip(configuration.testSkip());
      packetHeaderAnalyzer_.reset(analyzer);
      break;
    }
  case Application::DecoderConfiguration::FIXED_HEADER:
    {
      Codecs::FixedSizeHeaderAnalyzer * fixedSizeHeaderAnalyzer = new Codecs::FixedSizeHeaderAnalyzer(
        configuration.packetHeaderMessageSizeBytes(),
        configuration.packetHeaderBigEndian(),
        configuration.packetHeaderPrefixCount(),
        configuration.packetHeaderSuffixCount());
      fixedSizeHeaderAnalyzer->setTestSkip(configuration.testSkip());
      packetHeaderAnalyzer_.reset(fixedSizeHeaderAnalyzer);
      break;
    }
  case Application::DecoderConfiguration::FAST_HEADER:
    {
      packetHeaderAnalyzer_.reset(new Codecs::FastEncodedHeaderAnalyzer(
        configuration.packetHeaderPrefixCount(),
        configuration.packetHeaderSuffixCount(),
        configuration.packetHeaderMessageSizeBytes() != 0)); // true if header contains message size
      break;
    }
  }

  switch(configuration.messageHeaderType())
  {
  case Application::DecoderConfiguration::NO_HEADER:
    {
      Codecs::NoHeaderAnalyzer * analyzer = new Codecs::NoHeaderAnalyzer;
      analyzer->setTestSkip(configuration.testSkip());
      messageHeaderAnalyzer_.reset(analyzer);
      break;
    }
  case Application::DecoderConfiguration::FIXED_HEADER:
    {
      Codecs::FixedSizeHeaderAnalyzer * fixedSizeHeaderAnalyzer = new Codecs::FixedSizeHeaderAnalyzer(
        configuration.messageHeaderMessageSizeBytes(),
        configuration.messageHeaderBigEndian(),
        configuration.messageHeaderPrefixCount(),
        configuration.messageHeaderSuffixCount());
      fixedSizeHeaderAnalyzer->setTestSkip(configuration.testSkip());
      messageHeaderAnalyzer_.reset(fixedSizeHeaderAnalyzer);
      break;
    }
  case Application::DecoderConfiguration::FAST_HEADER:
    {
      messageHeaderAnalyzer_.reset(new Codecs::FastEncodedHeaderAnalyzer(
        configuration.messageHeaderPrefixCount(),
        configuration.messageHeaderSuffixCount(),
        configuration.messageHeaderMessageSizeBytes() != 0)); // true if header contains message size
      break;
    }
  }

  // echoFile_ stays null when no echo file is configured, and every assembler
  // branch used to pass *echoFile_ regardless. The null lands back in
  // DataSource::getEcho()'s documented "0 means no echo", so it worked, but
  // forming a reference from a null lvalue is undefined and a compiler is
  // entitled to assume a reference is never null.
  const auto applyEcho = [this, &configuration](auto * pAssembler)
  {
    if(echoFile_ != nullptr)
    {
      pAssembler->setEcho(
        *echoFile_,
        static_cast<Codecs::DataSource::EchoType>(configuration.echoType()),
        configuration.echoMessage(),
        configuration.echoField());
    }
  };

  switch(configuration.assemblerType())
  {
  case Application::DecoderConfiguration::MESSAGE_PER_PACKET_ASSEMBLER:
    {
      Codecs::MessagePerPacketAssembler * pAssembler = new Codecs::MessagePerPacketAssembler(
        registry_,
        *packetHeaderAnalyzer_,
        *messageHeaderAnalyzer_,
        builder);
      assembler_.reset(pAssembler);
      applyEcho(pAssembler);
      pAssembler->setMessageLimit(configuration.head());
      break;
    }
  case Application::DecoderConfiguration::STREAMING_ASSEMBLER:
    {
      Codecs::StreamingAssembler * pAssembler = new Codecs::StreamingAssembler(
        registry_,
        *messageHeaderAnalyzer_,
        builder,
        configuration.waitForCompleteMessage());
      assembler_.reset(pAssembler);
      applyEcho(pAssembler);
      pAssembler->setMessageLimit(configuration.head());
      break;
    }
  default:
    {
      // Assembler type not specified:  Choose an appropriate default assembler
      switch(configuration.receiverType())
      {
      case Application::DecoderConfiguration::MULTICAST_RECEIVER:
      case Application::DecoderConfiguration::PCAPFILE_RECEIVER:
      case Application::DecoderConfiguration::BUFFER_RECEIVER:
        {
          Codecs::MessagePerPacketAssembler * pAssembler = new Codecs::MessagePerPacketAssembler(
            registry_,
            *packetHeaderAnalyzer_,
            *messageHeaderAnalyzer_,
            builder);
          assembler_.reset(pAssembler);
          applyEcho(pAssembler);
          pAssembler->setMessageLimit(configuration.head());
          break;
        }
      case Application::DecoderConfiguration::TCP_RECEIVER:
      case Application::DecoderConfiguration::RAWFILE_RECEIVER:
      case Application::DecoderConfiguration::BUFFERED_RAWFILE_RECEIVER:
      case Application::DecoderConfiguration::ASYNCHRONOUS_FILE_RECEIVER:
        {
          Codecs::StreamingAssembler * pAssembler = new Codecs::StreamingAssembler(
            registry_,
            *messageHeaderAnalyzer_,
            builder,
            configuration.waitForCompleteMessage());
          assembler_.reset(pAssembler);
          applyEcho(pAssembler);
          pAssembler->setMessageLimit(configuration.head());
          break;
        }
      default:
        {
          std::stringstream msg;
          msg << "DecoderConnection: Cannot determine what type of assembler to use.";
          throw std::invalid_argument(msg.str());
        }
      }
    }
  }

  assembler_->setReset(configuration.reset());
  assembler_->setStrict(configuration.strict());

  switch(configuration.receiverType())
  {
  case Application::DecoderConfiguration::MULTICAST_RECEIVER:
    {
      if(configuration.sourceSpecificMulticast())
      {
        createSourceSpecificMulticastReceiver(configuration);
      }
      else
      {
        Communication::MulticastReceiver * receiver;
        if(configuration.privateIOService())
        {
          ioService_.reset(new asio::io_context);
          receiver = new Communication::MulticastReceiver(*ioService_);
        }
        else
        {
          receiver = new Communication::MulticastReceiver();
        }
        receiver_.reset(receiver);
        for(size_t nFeed = 0; nFeed < configuration.multicastCount(); ++nFeed)
        {
          receiver->addFeed(
            configuration.multicastName(nFeed),
            configuration.multicastGroupIP(nFeed),
            configuration.listenInterfaceIP(nFeed),
            configuration.multicastBindIP(nFeed),
            configuration.portNumber(nFeed)
            );
        }
      }
      break;
    }
  case Application::DecoderConfiguration::TCP_RECEIVER:
    {
      if(configuration.privateIOService())
      {
        ioService_.reset(new asio::io_context);
          receiver_.reset(new Communication::TCPReceiver(
            *ioService_,
            configuration.hostName(),
            configuration.portName()));
      }
      else
      {
        receiver_.reset(new Communication::TCPReceiver(
          configuration.hostName(),
          configuration.portName()));
      }
      break;
    }
  case Application::DecoderConfiguration::RAWFILE_RECEIVER:
    {
      receiver_.reset(new Communication::RawFileReceiver(
        *fastFile_));
      break;
    }
  case Application::DecoderConfiguration::BUFFERED_RAWFILE_RECEIVER:
    {
      receiver_.reset(new Communication::BufferedRawFileReceiver(
        *fastFile_));
      break;
    }
  case Application::DecoderConfiguration::PCAPFILE_RECEIVER:
    {
      receiver_.reset(new Communication::PCapFileReceiver(
        configuration.pcapFileName()));
      break;
    }
  case Application::DecoderConfiguration::ASYNCHRONOUS_FILE_RECEIVER:
    {
      unsigned long attributes = 0;
      try
      {
        std::string extraStr;
        if(configuration.getExtra("INPUT_ATTRIBUTES", extraStr))
        {
          std::istringstream extra(extraStr);
          extra >> std::hex >> attributes;
        }
      }
      catch (...)
      {
        ; // for now ignore this
      }

      receiver_.reset(new Communication::AsynchFileReceiver(
        configuration.fastFileName(),
        attributes));
      break;
    }
  case Application::DecoderConfiguration::BUFFER_RECEIVER:
    {
      receiver_.reset(new Communication::BufferReceiver);
      break;
    }
  default:
    {
      std::stringstream msg;
      msg << "DecoderConnection: Cannot determine what type of receiver to use.";
      throw std::invalid_argument(msg.str());
    }
  }

  // A receiver that fails to initialize never allocates buffers and never
  // arms a read, so running its event loop waits forever for data that cannot
  // arrive.  Discarding this result turned an unreadable capture file into a
  // hang; a batch or CI caller needs it to be a non-zero exit instead.
  if(!receiver_->start(*assembler_, configuration.bufferSize(), configuration.bufferCount()))
  {
    throw UsageError(
      "Cannot start connection",
      "The receiver could not be initialized. Check that the input source exists and is readable.");
  }
}

Codecs::Decoder &
DecoderConnection::decoder() const
{
  if(!assembler_)
  {
    throw UsageError("Coding Error","Using DecoderConnection decoder before it is configured.");
  }
  return assembler_->decoder();
}

