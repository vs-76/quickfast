// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//

#include <Examples/ExamplesPch.h>
#include "JsonMessageConsumer.h"
#include <Examples/ConsoleLock.h>

using namespace QuickFAST;
using namespace Examples;

JsonMessageConsumer::JsonMessageConsumer(
  std::ostream & out,
  const Messages::JsonOptions & options,
  bool silent)
  : formatter_(options)
  , out_(out)
  , recordCount_(0)
  , logLevel_(Common::Logger::QF_LOG_WARNING)
  , silent_(silent)
{
}

JsonMessageConsumer::~JsonMessageConsumer()
{
}

void
JsonMessageConsumer::setLogLevel(Common::Logger::LogLevel level)
{
  logLevel_ = level;
}

bool
JsonMessageConsumer::wantLog(unsigned short level)
{
  return level <= logLevel_;
}

bool
JsonMessageConsumer::logMessage(unsigned short level, const std::string & logMessage)
{
  if(level <= logLevel_)
  {
    std::unique_lock<std::mutex> lock(ConsoleLock::consoleMutex);
    std::cerr << logMessage << std::endl;
  }
  return true;
}

bool
JsonMessageConsumer::reportDecodingError(const std::string & errorMessage)
{
  std::unique_lock<std::mutex> lock(ConsoleLock::consoleMutex);
  std::cerr << "Decoding error: " << errorMessage << std::endl;
  return false;
}

bool
JsonMessageConsumer::reportCommunicationError(const std::string & errorMessage)
{
  std::unique_lock<std::mutex> lock(ConsoleLock::consoleMutex);
  std::cerr << "Communication error: " << errorMessage << std::endl;
  return false;
}

void
JsonMessageConsumer::decodingStarted()
{
}

void
JsonMessageConsumer::decodingStopped()
{
}

bool
JsonMessageConsumer::consumeMessage(Messages::Message & message)
{
  recordCount_ += 1;
  if(!silent_)
  {
    std::unique_lock<std::mutex> lock(ConsoleLock::consoleMutex);
    formatter_.formatMessage(message, out_);
    out_ << std::endl;
  }
  return true;
}
