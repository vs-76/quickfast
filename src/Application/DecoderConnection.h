// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef DECODERCONNECTION_H
#define DECODERCONNECTION_H
#include <Common/QuickFAST_Export.h>
#include <Messages/ValueMessageBuilder.h>

#include <Common/Exceptions.h>
#include <Codecs/TemplateRegistry_fwd.h>
#include <Codecs/HeaderAnalyzer_fwd.h>
#include <Codecs/Decoder_fwd.h>
#include <Communication/Assembler_fwd.h>
#include <Communication/Receiver.h>
#include <Communication/AsioService_fwd.h>
#include <Application/DecoderConfiguration.h>

namespace QuickFAST{
  namespace Application{
    /// @brief Support a single source of FAST encoded data.
    ///
    /// Each source of FAST encoded data should be supported by a separate instance of DecoderConnection.
    /// Examples of sources include a single multicast group; a FAST encoded data file; etc.
    class QuickFAST_Export DecoderConnection
    {
    public:
      DecoderConnection();
      ~DecoderConnection();
      /// @brief call this before calling configure if you want to share a prebuilt template registry
      /// @param registry the registry to use
      void setTemplateRegistry(Codecs::TemplateRegistryPtr registry);

      /// @brief Configure the connection for use
      /// @param builder accepts the decoded fields
      /// @param configuration contains configuration parameters
      void configure(Messages::ValueMessageBuilder & builder, Application::DecoderConfiguration &configuration);

      //////////////////////////////////////////////
      // forward the following calls to the receiver
      // It was not obvious how to actually start the connection before:

      /// @brief run the event loop in this thread
      ///
      /// Exceptions are caught, logged, and ignored.  The event loop continues.
      void run()
      {
        receiver_->run();
      }

      /// @brief run the event loop until one event is handled.
      void run_one()
      {
        receiver_->run_one();
      }

      /// @brief execute any ready event handlers than return.
      size_t poll()
      {
        return receiver_->poll();
      }

      /// @brief execute at most one ready event handler than return.
      size_t poll_one()
      {
        return receiver_->poll_one();
      }

      /// @brief create additional threads to run the event loop
      void runThreads(size_t threadCount = 0, bool useThisThread = true)
      {
        receiver_->runThreads(threadCount, useThisThread);
      }

      /// @brief join all additional threads after calling stopService()
      ///
      /// If stop() has not been called, this will block "forever".
      void joinThreads()
      {
        receiver_->joinThreads();
      }

      /// @brief Reuse AsioService after calling stop and joinThreads
      ///
      /// The sequence of calls should be:
      /// while(whatever)
      /// {
      ///   runThreads
      ///   stop
      ///   joinThreads
      ///    resetService
      /// }
      /// Note that if the AsioService is being shared this will reset "everybody"
      /// if you need to shut down/restart specific parts of the system, arrange to use
      /// separate AsioService (see the AsyncReceiver constructor that takes an AsioService as an argument.
      void resetService()
      {
        receiver_->resetService();
      }
      // end calls forwarded to the receiver
      //////////////////////////////////////

      /// @brief provide access to the template registry
      Codecs::TemplateRegistryPtr & registry()
      {
        if(!registry_)
        {
          throw UsageError("Coding Error","Using DecoderConnection registry before it is configured.");
        }
        return registry_;
      }

      /// @brief Forward compatibility
      /// @deprecated use packetHeaderAnalyzer or messageHeaderAnalyzer
      Codecs::HeaderAnalyzer & analyzer()const
      {
        return packetHeaderAnalyzer();
      }

      /// @brief Access the packet header analyzer.
      Codecs::HeaderAnalyzer & packetHeaderAnalyzer() const
      {
        if(!packetHeaderAnalyzer_)
        {
          throw UsageError("Coding Error","Using DecoderConnection packet header analyzer before it is configured.");
        }
        return *packetHeaderAnalyzer_;
      }

      /// @brief Access the message header analyzer.
      Codecs::HeaderAnalyzer & messageHeaderAnalyzer() const
      {
        if(!messageHeaderAnalyzer_)
        {
          throw UsageError("Coding Error","Using DecoderConnection message header analyzer before it is configured.");
        }
        return *messageHeaderAnalyzer_;
      }

      /// @brief Access the message assembler.
      Communication::Assembler & assembler()const
      {
        if(!assembler_)
        {
          throw UsageError("Coding Error","Using DecoderConnection assembler before it is configured.");
        }
        return *assembler_;
      }

      /// @brief Access the data receiver.
      Communication::Receiver & receiver()const
      {
        if(!receiver_)
        {
          throw UsageError("Coding Error","Using DecoderConnection receiver before it is configured.");
        }
        return *receiver_;
      }

      /// @brief Access the decoder.
      Codecs::Decoder & decoder() const;

    private:
      /// Build the receiver for a multicast input whose feeds name their senders.
      void createSourceSpecificMulticastReceiver(
        const Application::DecoderConfiguration & configuration);

      // Each of these streams may be one this connection opened or one it is
      // merely borrowing -- cin, cout or cerr. That distinction used to be
      // carried by a bool beside each raw pointer, and fastFile_ never got
      // one, so -file cin ended in delete &std::cin. Ownership now lives in
      // the type, and the view pointers below say nothing about lifetime.
      std::unique_ptr<std::istream> ownedFastFile_;
      std::unique_ptr<std::ostream> ownedEchoFile_;
      std::unique_ptr<std::ostream> ownedVerboseFile_;

      std::istream * fastFile_ = nullptr;
      std::ostream * echoFile_ = nullptr;
      std::ostream * verboseFile_ = nullptr;

      Codecs::TemplateRegistryPtr registry_;
      std::unique_ptr<asio::io_context> ioService_;
      std::unique_ptr<Codecs::HeaderAnalyzer> packetHeaderAnalyzer_;
      std::unique_ptr<Codecs::HeaderAnalyzer> messageHeaderAnalyzer_;
      std::unique_ptr<Communication::Assembler> assembler_;
      std::unique_ptr<Communication::Receiver> receiver_;

    };
  }
}

#endif // DECODERCONNECTION_H
