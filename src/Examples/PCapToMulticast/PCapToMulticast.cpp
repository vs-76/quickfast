// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Examples/ExamplesPch.h>
#include "PCapToMulticast.h"
#include <Examples/StopWatch.h>
#include <Common/Types.h>
#include <Common/LexicalCast.h>
using namespace QuickFAST;
using namespace Examples;

namespace {
  void waitForEnter()
  {
    std::cout << "Hit Enter to continue:" << std::flush;
    char c = (char)std::cin.get();
    if(c == 'q')
    {
      exit(1);
    }
  }
}

PCapToMulticast::PCapToMulticast()
: portNumber_(13014)
, sendAddress_("224.1.2.133")
, sendCount_(1)
, sendMicroseconds_(500)
, burst_(1)
, pauseEveryPass_(false)
, pauseEveryMessage_(false)
, verbose_(false)
, socket_(ioService_)
, strand_(ioService_)
, timer_(ioService_)
, nPass_(0)
, nMsg_(0)
, totalMessageCount_(0)
{
}

PCapToMulticast::~PCapToMulticast()
{
}


bool
PCapToMulticast::init(int argc, char * argv[])
{
  commandArgParser_.addHandler(this);
  return commandArgParser_.parse(argc, argv);
}

int
PCapToMulticast::parseSingleArg(int argc, char * argv[])
{
  int consumed = 0;
  std::string opt(argv[0]);
  try
  {
    if(opt == "-f")
    {
      dataFileName_ = argv[1];
      consumed = 2;
    }
    else if(opt == "-p" && argc > 1)
    {
      portNumber_ = QuickFAST::lexical_cast<unsigned short>(argv[1]);
      consumed = 2;
    }
    else if(opt == "-r" && argc > 1)
    {
      size_t mps = QuickFAST::lexical_cast<size_t>(argv[1]);
      if(mps > 0)
      {
        sendMicroseconds_ = 1000000/mps;
        consumed = 2;
      }
      else
      {
        sendMicroseconds_ = 0;
        consumed = 2;
      }
    }
    else if(opt == "-b" && argc > 1)
    {
      burst_ = QuickFAST::lexical_cast<size_t>(argv[1]);
      consumed = 2;
    }
    else if(opt == "-c" && argc > 1)
    {
      sendCount_ = QuickFAST::lexical_cast<size_t>(argv[1]);
      consumed = 2;
    }
    else if(opt == "-a" && argc > 1)
    {
      sendAddress_ = argv[1];
      consumed = 2;
    }
    else if(opt == "-pausemessage")
    {
      pauseEveryMessage_ = true;
      consumed = 1;
    }
    else if(opt == "-pausepass")
    {
      pauseEveryPass_ = true;
      consumed = 1;
    }
    else if(opt == "-v")
    {
      verbose_ = !verbose_;
      consumed = 1;
    }
  }
  catch (std::exception & ex)
  {
    // Returning 0 here would make the parser announce "Unknown argument" for
    // an option that exists and is spelled correctly.
    std::cerr << opt;
    if(argc > 1)
    {
      std::cerr << " \"" << argv[1] << "\"";
    }
    std::cerr << ": " << ex.what() << std::endl;
    consumed = Application::CommandArgHandler::ARGUMENT_VALUE_ERROR;
  }
  return consumed;
}

void
PCapToMulticast::usage(std::ostream & out) const
{
  out << "  -a dotted_ip       : Multicast send address (default is 239.255.0.1)" << std::endl;
  out << "  -p port            : Multicast port number (default 30001)" << std::endl;
  out << "  -f datafile        : File containing PCAP format capture of a multicast feed." << std::endl;
  out << "  -r burst/sec       : Rate at which to send bursts of messages expressed as bursts per second (default = 2000)" << std::endl;
  out << "                     : zero means send continuously." << std::endl;
  out << "  -b msg/burst       : Messages per burst(default = 1)" << std::endl;
  out << "  -c count           : How many times to send the file (passes)" << std::endl;
  out << "                       (default 1; 0 means forever.)" << std::endl;
  out << "  -pausemessage      : Wait for 'Enter' before every message." << std::endl;
  out << "  -pausepass         : Wait for 'Enter' before every pass." << std::endl;
  out << "  -packetchecksum n  : size of packet checksum (default is 4)" << std::endl;
  out << "  -v                 : Noise to the console while it runs" << std::endl;
}

bool
PCapToMulticast::applyArgs()
{
  bool ok = true;
  try
  {
    if(dataFileName_.empty())
    {
      ok = false;
      std::cerr << "ERROR: -f [datafile] option is required." << std::endl;
      commandArgParser_.usage(std::cerr);
    }
    ok = ok && pcapReader_.open(dataFileName_.c_str());// for debugging dump to->, &std::cout);

    multicastAddress_ = asio::ip::make_address(sendAddress_);
    endpoint_ = asio::ip::udp::endpoint(multicastAddress_, portNumber_);
    socket_.open(endpoint_.protocol());
    std::cout << "Opening multicast group: " << endpoint_.address().to_string() << ':' << endpoint_.port() << std::endl;
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    ok = false;
  }
  return ok;
}

int
PCapToMulticast::run()
{
  try
  {
    if(verbose_)
    {
      std::cout << " Configuring multicast: " << multicastAddress_ << '|' << sendAddress_ << ':' << portNumber_ << std::endl;
    }

    asio::dispatch(strand_,
        [this]{ this->sendBurst(); });
    StopWatch lapse;
    this->ioService_.run();
    unsigned long sendLapse = lapse.freeze();
    std::cout << "sent "
      << totalMessageCount_
      << " messages in "
      << std::fixed << std::setprecision(3)
      << sendLapse
      << " milliseonds. [";
    std::cout << std::fixed << std::setprecision(3)
      << double(sendLapse)/double(totalMessageCount_) << " msec/message. = "
      << std::fixed << std::setprecision(0)
      << 1000. * double(totalMessageCount_)/double(sendLapse) << " message/second.]"
      << std::endl;

#ifdef _WIN32
    // On WIN32 if sender closes the socket before a localhost receiver
    // reads all data, any unread data is thrown away.
    // This ugly sleep lets the dust settle.
    Sleep(1000);
#endif // _WIN32
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}

void
PCapToMulticast::sendBurst()
{
  try
  {
    // set the next timeout
    if(sendMicroseconds_ != 0)
    {
      timer_.expires_after(std::chrono::microseconds(sendMicroseconds_));
      timer_.async_wait(
        asio::bind_executor(strand_,
          [this](const asio::error_code&){ this->sendBurst(); })
        );
    }

    for(size_t nBurstMsg = 0; nBurstMsg < burst_; ++nBurstMsg)
    {
      if(!pcapReader_.good())
      {


        // completed a pass;  count it and see if we should stop or pause
        nPass_ += 1;
        if(nPass_ >= sendCount_ && sendCount_ != 0)
        {
          ioService_.stop();
          return;
        }
        if(verbose_)
        {
          std::cout << "Begin pass #" << nPass_ << " of " << sendCount_ << std::endl;
        }
        if(pauseEveryPass_)
        {
          waitForEnter();
        }
        nMsg_ = 0;
        pcapReader_.rewind();
      }

      // then send this message

      const unsigned char * msgBuffer = 0;
      size_t bytesRead = 0;
      if(!pcapReader_.read(msgBuffer, bytesRead))
      {
        // Without this the end of a pass sent a zero length datagram before
        // good() was next tested.
        return;
      }

      nMsg_ += 1;
      totalMessageCount_ += 1;

      if(verbose_)
      {
        std::cout << "Send message #" << nMsg_
          << std::endl;
        std::cout << "         to: " << multicastAddress_ << '|' << sendAddress_ << ':' << portNumber_ << std::endl;

        std::cout << "Msg:";
        for(size_t nByte = 0; /*nByte < 10 && */ nByte < bytesRead; ++nByte)
        {
          if(nByte % 16 == 0) std::cout << std::endl;
          unsigned short shortByte = static_cast<unsigned short>(msgBuffer[nByte]) & 0xFF;
          std::cout << ' ' << std::hex << std::setw(2)<< std::setfill('0') << shortByte << std::dec;
        }
        std::cout << std::endl;
      }

      if(pauseEveryMessage_)
      {
        waitForEnter();
      }
      socket_.send_to(asio::buffer(msgBuffer, bytesRead), endpoint_);
    }
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    ioService_.stop();
  }
}


void
PCapToMulticast::fini()
{
}

