// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A wrong value for a valid option was reported as an unknown option.
//
// The handler caught the conversion failure, printed a diagnostic, and then
// returned "consumed nothing" -- which the parser reads as "argv[0] was not
// recognized". So the two lines contradicted each other, and the second one
// came immediately above the usage dump that draws the eye. A user concludes
// the option was renamed and goes looking for its replacement.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/CommandArgParser.h>
#include <Application/CommandArgHandler.h>
#include <Common/LexicalCast.h>

using namespace QuickFAST;

namespace
{
  /// @brief A handler with one valid option that takes a number.
  class NumericOptionHandler : public Application::CommandArgHandler
  {
  public:
    int parseSingleArg(int argc, char * argv[]) override
    {
      const std::string opt(argv[0]);
      try
      {
        if(opt == "-limit" && argc > 1)
        {
          limit_ = QuickFAST::lexical_cast<size_t>(argv[1]);
          return 2;
        }
      }
      catch(const std::exception & ex)
      {
        std::cerr << ex.what() << " while interpreting " << opt << std::endl;
        return Application::CommandArgHandler::ARGUMENT_VALUE_ERROR;
      }
      return 0;
    }

    void usage(std::ostream & out) const override
    {
      out << "  -limit n" << std::endl;
    }

    bool applyArgs() override
    {
      return true;
    }

    size_t limit_ = 0;
  };

  /// @brief Run the parser over one command line, capturing what it printed.
  bool parse(NumericOptionHandler & handler,
    const std::vector<std::string> & words, std::string & errorOutput)
  {
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>("program"));
    for(const std::string & word : words)
    {
      argv.push_back(const_cast<char *>(word.c_str()));
    }

    Application::CommandArgParser parser;
    parser.addHandler(&handler);

    std::stringstream captured;
    std::streambuf * saved = std::cerr.rdbuf(captured.rdbuf());
    const bool result =
      parser.parse(static_cast<int>(argv.size()), &argv[0]);
    std::cerr.rdbuf(saved);

    errorOutput = captured.str();
    return result;
  }
}

/// @brief A bad value must not be described as an unrecognized option.
TEST(QuickFAST, testBadOptionValueIsNotReportedAsUnknownOption)
{
  NumericOptionHandler handler;
  std::string output;
  EXPECT_FALSE(parse(handler, {"-limit", "abc"}, output));

  EXPECT_NE(std::string::npos, output.find("while interpreting -limit"))
    << output;
  EXPECT_EQ(std::string::npos, output.find("Unknown argument"))
    << "a valid option was described as unknown:\n" << output;
}

/// @brief A genuinely unknown option is still described as unknown.
TEST(QuickFAST, testUnknownOptionIsStillReported)
{
  NumericOptionHandler handler;
  std::string output;
  EXPECT_FALSE(parse(handler, {"-nosuchoption"}, output));

  EXPECT_NE(std::string::npos, output.find("Unknown argument")) << output;
}

/// @brief A valid option with a valid value is still accepted.
TEST(QuickFAST, testValidOptionValueIsAccepted)
{
  NumericOptionHandler handler;
  std::string output;
  EXPECT_TRUE(parse(handler, {"-limit", "42"}, output));
  EXPECT_EQ(42u, handler.limit_);
  EXPECT_EQ("", output);
}
