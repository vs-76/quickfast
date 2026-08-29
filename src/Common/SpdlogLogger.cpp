// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "SpdlogLogger.h"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Common;

namespace
{
  spdlog::level::level_enum
  toSpdlogLevel(Logger::LogLevel level)
  {
    switch(level)
    {
    case Logger::QF_LOG_FATAL:
      return spdlog::level::critical;
    case Logger::QF_LOG_SERIOUS:
      return spdlog::level::err;
    case Logger::QF_LOG_WARNING:
      return spdlog::level::warn;
    case Logger::QF_LOG_INFO:
      return spdlog::level::info;
    case Logger::QF_LOG_VERBOSE:
    default:
      return spdlog::level::debug;
    }
  }
}

SpdlogLogger::SpdlogLogger(
  std::shared_ptr<spdlog::logger> logger,
  bool continueOnDecodingError,
  bool continueOnCommunicationError)
  : logger_(std::move(logger))
  , continueOnDecodingError_(continueOnDecodingError)
  , continueOnCommunicationError_(continueOnCommunicationError)
{
  if(!logger_)
  {
    throw std::invalid_argument("SpdlogLogger requires a non-null spdlog::logger");
  }
}

SpdlogLogger::~SpdlogLogger() = default;

bool
SpdlogLogger::wantLog(LogLevel level)
{
  return logger_->should_log(toSpdlogLevel(level));
}

bool
SpdlogLogger::logMessage(LogLevel level, const std::string & message)
{
  logger_->log(toSpdlogLevel(level), "{}", message);
  return true;
}

bool
SpdlogLogger::reportDecodingError(const std::string & message)
{
  logger_->error("{}", message);
  return continueOnDecodingError_;
}

bool
SpdlogLogger::reportCommunicationError(const std::string & message)
{
  logger_->error("{}", message);
  return continueOnCommunicationError_;
}
