// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef JSONMESSAGECONSUMER_H
#define JSONMESSAGECONSUMER_H
#include <Codecs/MessageConsumer.h>
#include <Messages/MessageToJson.h>
#include <Messages/JsonOptions.h>

namespace QuickFAST{
  namespace Examples{

    /// @brief MessageConsumer that writes each decoded message as one JSON line.
    ///
    /// @par Example
    /// @code
    /// QuickFAST::Examples::JsonMessageConsumer handler(std::cout);
    /// QuickFAST::Codecs::GenericMessageBuilder builder(handler);
    /// connection.configure(builder, configuration);
    /// connection.run();
    /// std::cerr << handler.recordCount() << " messages" << std::endl;
    /// @endcode
    class JsonMessageConsumer : public Codecs::MessageConsumer
    {
    public:
      /// @param out destination for NDJSON lines
      /// @param options conversion options
      /// @param silent when true, messages are counted but not written
      explicit JsonMessageConsumer(
        std::ostream & out,
        const Messages::JsonOptions & options = Messages::JsonOptions(),
        bool silent = false);
      virtual ~JsonMessageConsumer();

      /// @brief Set the lowest level this consumer will accept via wantLog().
      /// @param level the threshold level
      void setLogLevel(Common::Logger::LogLevel level);

      virtual bool consumeMessage(Messages::Message & message);
      virtual bool wantLog(unsigned short level);
      virtual bool logMessage(unsigned short level, const std::string & logMessage);
      virtual bool reportDecodingError(const std::string & errorMessage);
      virtual bool reportCommunicationError(const std::string & errorMessage);
      virtual void decodingStarted();
      virtual void decodingStopped();

      /// @returns how many messages have been consumed so far
      size_t recordCount() const
      {
        return recordCount_;
      }

    private:
      Messages::MessageToJson formatter_;
      std::ostream & out_;
      size_t recordCount_;
      Common::Logger::LogLevel logLevel_;
      bool silent_;
    };
  }
}
#endif /* JSONMESSAGECONSUMER_H */
