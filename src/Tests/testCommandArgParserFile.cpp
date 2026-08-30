// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// CommandArgParser's options-file path tokenizes escapes and quotes. A broken
// file or a valid one must not silently drop configuration.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/CommandArgParser.h>
#include <Application/CommandArgHandler.h>
#include <cstdio>
#include <fstream>

using namespace QuickFAST;

namespace
{
  class FlagHandler : public Application::CommandArgHandler
  {
  public:
    int parseSingleArg(int argc, char * argv[]) override
    {
      const std::string opt(argv[0]);
      if(opt == "-flag")
      {
        seen_ = true;
        return 1;
      }
      if(opt == "-name" && argc > 1)
      {
        name_ = argv[1];
        return 2;
      }
      return 0;
    }
    void usage(std::ostream & out) const override
    {
      out << "  -flag\n  -name word\n";
    }
    bool applyArgs() override { return true; }

    bool seen_ = false;
    std::string name_;
  };

  std::string writeTemp(const std::string & contents)
  {
    const std::string path = "testCommandArgParserOptions.tmp";
    std::ofstream out(path.c_str(), std::ios::trunc);
    out << contents;
    return path;
  }
}

/// @brief -options must load tokens, including quoted and escaped values.
TEST(QuickFAST, testCommandArgParserReadsOptionsFile)
{
  const std::string path = writeTemp("-flag -name \"hello world\" -name escaped\\ value\n");
  FlagHandler handler;
  Application::CommandArgParser parser;
  parser.addHandler(&handler);

  char prog[] = "prog";
  char opt[] = "-options";
  std::vector<char> pathBuf(path.begin(), path.end());
  pathBuf.push_back('\0');
  char * argv[] = {prog, opt, pathBuf.data()};

  std::stringstream captured;
  std::streambuf * saved = std::cerr.rdbuf(captured.rdbuf());
  EXPECT_TRUE(parser.parse(3, argv));
  std::cerr.rdbuf(saved);

  EXPECT_TRUE(handler.seen_);
  EXPECT_EQ("escaped value", handler.name_);
  std::remove(path.c_str());
}

/// @brief An empty options file is a successful no-op.
TEST(QuickFAST, testCommandArgParserAcceptsEmptyOptionsFile)
{
  const std::string path = writeTemp("");
  FlagHandler handler;
  Application::CommandArgParser parser;
  parser.addHandler(&handler);

  char prog[] = "prog";
  char opt[] = "-options";
  std::vector<char> pathBuf(path.begin(), path.end());
  pathBuf.push_back('\0');
  char * argv[] = {prog, opt, pathBuf.data()};
  EXPECT_TRUE(parser.parse(3, argv));
  EXPECT_FALSE(handler.seen_);
  std::remove(path.c_str());
}
