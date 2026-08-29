// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Numeric CLI options go through lexical_cast. A negative or non-numeric value
// must fail at parse time, not become SIZE_MAX buffers and hang the receiver.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/DecoderConfiguration.h>
#include <Application/CommandArgParser.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  int parseOne(Application::DecoderConfiguration & configuration,
    const std::vector<std::string> & words)
  {
    std::vector<char *> argv;
    for(const std::string & word : words)
    {
      argv.push_back(const_cast<char *>(word.c_str()));
    }
    return configuration.parseSingleArg(
      static_cast<int>(argv.size()), &argv[0]);
  }
}

/// @brief Negative and non-numeric buffer options must be refused.
TEST(QuickFAST, testConfigurationRejectsBadBufferOptionValues)
{
  Application::DecoderConfiguration configuration;

  EXPECT_THROW(
    (void)parseOne(configuration, {"-buffers", "-1"}),
    std::invalid_argument);
  EXPECT_THROW(
    (void)parseOne(configuration, {"-buffers", "abc"}),
    std::invalid_argument);
  EXPECT_THROW(
    (void)parseOne(configuration, {"-buffersize", "-5"}),
    std::invalid_argument);
  EXPECT_THROW(
    (void)parseOne(configuration, {"-buffersize", "xyz"}),
    std::invalid_argument);
  EXPECT_THROW(
    (void)parseOne(configuration, {"-limit", "-1"}),
    std::invalid_argument);
}

/// @brief A missing value must not look like a successful consume.
TEST(QuickFAST, testConfigurationDoesNotConsumeOptionWithoutValue)
{
  Application::DecoderConfiguration configuration;
  EXPECT_EQ(0, parseOne(configuration, {"-buffers"}));
  EXPECT_EQ(0, parseOne(configuration, {"-file"}));
  EXPECT_EQ(0, parseOne(configuration, {"-t"}));
}

/// @brief Valid buffer options still take effect.
TEST(QuickFAST, testConfigurationAcceptsValidBufferOptions)
{
  Application::DecoderConfiguration configuration;
  EXPECT_EQ(2, parseOne(configuration, {"-buffers", "4"}));
  EXPECT_EQ(4u, configuration.bufferCount());
  EXPECT_EQ(2, parseOne(configuration, {"-buffersize", "2048"}));
  EXPECT_EQ(2048u, configuration.bufferSize());
}

/// @brief CommandArgParser still surfaces -V and unknown options cleanly.
TEST(QuickFAST, testCommandArgParserVersionAndUnknownOption)
{
  Application::DecoderConfiguration configuration;
  Application::CommandArgParser parser;
  // DecoderConfiguration is not a CommandArgHandler; use an empty parser for
  // the built-in -V / -? paths that live on the parser itself.
  char prog[] = "prog";
  char version[] = "-V";
  char * argv[] = {prog, version};
  std::stringstream captured;
  std::streambuf * saved = std::cerr.rdbuf(captured.rdbuf());
  EXPECT_FALSE(parser.parse(2, argv));
  std::cerr.rdbuf(saved);
  EXPECT_FALSE(captured.str().empty());

  char unknown[] = "-nosuch";
  char * bad[] = {prog, unknown};
  std::stringstream badOut;
  saved = std::cerr.rdbuf(badOut.rdbuf());
  EXPECT_FALSE(parser.parse(2, bad));
  std::cerr.rdbuf(saved);
  EXPECT_NE(std::string::npos, badOut.str().find("Unknown argument"));
}

/// @brief An options file that cannot be opened must fail closed.
TEST(QuickFAST, testCommandArgParserRejectsMissingOptionsFile)
{
  Application::CommandArgParser parser;
  char prog[] = "prog";
  char opt[] = "-options";
  char missing[] = "/no/such/quickfast-options-file-xyz";
  char * argv[] = {prog, opt, missing};

  std::stringstream captured;
  std::streambuf * saved = std::cerr.rdbuf(captured.rdbuf());
  EXPECT_FALSE(parser.parse(3, argv));
  std::cerr.rdbuf(saved);
  EXPECT_NE(std::string::npos, captured.str().find("Cannot open options file"));
}
