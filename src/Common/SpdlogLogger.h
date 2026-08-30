// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef QUICKFAST_SPDLOGLOGGER_H
#define QUICKFAST_SPDLOGLOGGER_H

#ifndef QUICKFAST_HAS_SPDLOG
# error "SpdlogLogger requires QUICKFAST_USE_SPDLOG=ON (QUICKFAST_HAS_SPDLOG)."
#endif

#include <Common/Logger.h>
#include <Common/QuickFAST_Export.h>
#include <memory>

namespace spdlog
{
  class logger;
}

namespace QuickFAST
{
  namespace Common
  {
    /// @brief Optional @c Logger adapter that forwards to an existing spdlog logger.
    ///
    /// Construct with a @c std::shared_ptr<spdlog::logger> from the application.
    /// Inject the same instance via message consumers and/or
    /// @c Communication::AsioService::setLogger.
    ///
    /// @par Example
    /// @code
    /// auto logger = spdlog::basic_logger_mt("quickfast", "quickfast.log");
    /// logger->set_level(spdlog::level::info);
    ///
    /// // continueOnDecodingError = false: stop the run on the first bad message.
    /// QuickFAST::Common::SpdlogLogger adapter(logger, false);
    /// service.setLogger(adapter);
    /// @endcode
    ///
    /// @see managed_file_sink_mt for a sink with rotation, gzip, and retention.
    class QuickFAST_Export SpdlogLogger : public Logger
    {
    public:
      /// @param logger Application-owned spdlog logger (must outlive this adapter's use).
      /// @param continueOnDecodingError Value returned from @c reportDecodingError.
      /// @param continueOnCommunicationError Value returned from @c reportCommunicationError.
      explicit SpdlogLogger(
        std::shared_ptr<spdlog::logger> logger,
        bool continueOnDecodingError = true,
        bool continueOnCommunicationError = true);

      ~SpdlogLogger() override;

      SpdlogLogger(const SpdlogLogger &) = delete;
      SpdlogLogger & operator=(const SpdlogLogger &) = delete;

      bool wantLog(LogLevel level) override;
      bool logMessage(LogLevel level, const std::string & message) override;
      bool reportDecodingError(const std::string & message) override;
      bool reportCommunicationError(const std::string & message) override;

    private:
      std::shared_ptr<spdlog::logger> logger_;
      bool continueOnDecodingError_;
      bool continueOnCommunicationError_;
    };
  }
}
#endif /* QUICKFAST_SPDLOGLOGGER_H */
