// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <sstream>

#include <Common/SpdlogLogger.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/ostream_sink.h>

using namespace QuickFAST;
using namespace QuickFAST::Common;

namespace
{
  struct CapturingLogger
  {
    std::ostringstream stream;
    std::shared_ptr<spdlog::logger> logger;

    CapturingLogger()
    {
      auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
      logger = std::make_shared<spdlog::logger>("quickfast_test", sink);
      logger->set_pattern("%v");
      logger->set_level(spdlog::level::info);
      logger->flush_on(spdlog::level::trace);
    }
  };
}

TEST(QuickFAST, SpdlogLoggerWantLogRespectsLevel)
{
  CapturingLogger capture;
  SpdlogLogger adapter(capture.logger);

  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_FATAL));
  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_SERIOUS));
  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_WARNING));
  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_INFO));
  EXPECT_FALSE(adapter.wantLog(Logger::QF_LOG_VERBOSE));

  capture.logger->set_level(spdlog::level::debug);
  EXPECT_TRUE(adapter.wantLog(Logger::QF_LOG_VERBOSE));
}

TEST(QuickFAST, SpdlogLoggerLogMessageForwards)
{
  CapturingLogger capture;
  SpdlogLogger adapter(capture.logger);

  EXPECT_TRUE(adapter.logMessage(Logger::QF_LOG_INFO, "hello-spdlog"));
  capture.logger->flush();
  EXPECT_NE(std::string::npos, capture.stream.str().find("hello-spdlog"));
}

TEST(QuickFAST, SpdlogLoggerReportContinueFlags)
{
  CapturingLogger capture;
  SpdlogLogger stopOnError(capture.logger, false, false);
  EXPECT_FALSE(stopOnError.reportDecodingError("decode-fail"));
  EXPECT_FALSE(stopOnError.reportCommunicationError("comm-fail"));

  CapturingLogger capture2;
  SpdlogLogger keepGoing(capture2.logger, true, true);
  EXPECT_TRUE(keepGoing.reportDecodingError("decode-ok-to-continue"));
  EXPECT_TRUE(keepGoing.reportCommunicationError("comm-ok-to-continue"));

  capture.logger->flush();
  capture2.logger->flush();
  EXPECT_NE(std::string::npos, capture.stream.str().find("decode-fail"));
  EXPECT_NE(std::string::npos, capture2.stream.str().find("comm-ok-to-continue"));
}

TEST(QuickFAST, SpdlogLoggerRejectsNullLogger)
{
  EXPECT_THROW(SpdlogLogger(nullptr), std::invalid_argument);
}
