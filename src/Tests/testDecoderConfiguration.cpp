// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Configuration must survive being copied, and -mname must name a feed.
//
// A misconfiguration that reports itself is a nuisance; one that reports a
// confident wrong answer is a fault. Both defects here are the second kind:
// the copy constructor dropped the feed list and a lazy initializer refilled
// it with a hard-coded group, and -multicast wrote feed zero whatever -mname
// had just named.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/DecoderConfiguration.h>

using namespace QuickFAST;

namespace
{
  /// @brief Drive the real command line parser, as an application would.
  void parse(Application::DecoderConfiguration & configuration,
    const std::vector<std::string> & words)
  {
    std::vector<char *> argv;
    for(const std::string & word : words)
    {
      argv.push_back(const_cast<char *>(word.c_str()));
    }

    int index = 0;
    while(index < static_cast<int>(argv.size()))
    {
      const int consumed = configuration.parseSingleArg(
        static_cast<int>(argv.size()) - index, &argv[index]);
      ASSERT_GT(consumed, 0) << "unrecognized option " << argv[index];
      index += consumed;
    }
  }
}

/// @brief A copied configuration must describe the same feeds as the original.
///
/// multicastFeeds_ was absent from the copy constructor's initializer list, so
/// the copy started empty; needMulticastFeed() then substituted a hard-coded
/// 224.1.2.133:13014. Since InterpretApplication builds every connection after
/// the first by copying, -connection subscribed each of them to a group nobody
/// asked for, and reported no error while doing it.
TEST(QuickFAST, testConfigurationCopyPreservesMulticastFeeds)
{
  Application::DecoderConfiguration original;
  parse(original, {"-multicast", "239.7.7.7:7777"});

  ASSERT_EQ(1u, original.multicastCount());
  ASSERT_EQ("239.7.7.7", original.multicastGroupIP(0));
  ASSERT_EQ(7777, original.portNumber(0));

  const Application::DecoderConfiguration copy(original);

  EXPECT_EQ(1u, copy.multicastCount());
  EXPECT_EQ("239.7.7.7", copy.multicastGroupIP(0));
  EXPECT_EQ(7777, copy.portNumber(0));
}

/// @brief Every scalar setting must survive the copy too.
///
/// asynchReads_ was the one member of the thirty-four the copy constructor did
/// mention whose initializer was not rhs.<member>. It is a throughput setting,
/// so losing it is a silent performance regression that no functional test
/// would show.
TEST(QuickFAST, testConfigurationCopyPreservesAsynchReads)
{
  Application::DecoderConfiguration original;
  original.setAsynchReads(true);
  ASSERT_TRUE(original.asynchReads());

  const Application::DecoderConfiguration copy(original);
  EXPECT_TRUE(copy.asynchReads());
}

/// @brief -mname must name the feed that the following -multicast configures.
///
/// The setters wrote index zero unconditionally, so the documented option
/// order produced two wrong feeds: the first overwritten by the second's
/// address, the second left with the placeholder it was created with.
TEST(QuickFAST, testMulticastOptionsConfigureTheNamedFeed)
{
  Application::DecoderConfiguration configuration;
  parse(configuration, {
    "-mname", "FEED_A", "-multicast", "224.1.1.1:1111", "-mlisten", "10.0.0.1",
    "-mname", "FEED_B", "-multicast", "224.2.2.2:2222", "-mlisten", "10.0.0.2"});

  ASSERT_EQ(2u, configuration.multicastCount());

  EXPECT_EQ("FEED_A", configuration.multicastName(0));
  EXPECT_EQ("224.1.1.1", configuration.multicastGroupIP(0));
  EXPECT_EQ(1111, configuration.portNumber(0));
  EXPECT_EQ("10.0.0.1", configuration.listenInterfaceIP(0));

  EXPECT_EQ("FEED_B", configuration.multicastName(1));
  EXPECT_EQ("224.2.2.2", configuration.multicastGroupIP(1));
  EXPECT_EQ(2222, configuration.portNumber(1));
  EXPECT_EQ("10.0.0.2", configuration.listenInterfaceIP(1));
}

/// @brief A single feed with no -mname still works, and still gets the default name.
TEST(QuickFAST, testUnnamedMulticastFeedIsUnaffected)
{
  Application::DecoderConfiguration configuration;
  parse(configuration, {"-multicast", "224.3.3.3:3333"});

  EXPECT_EQ(1u, configuration.multicastCount());
  EXPECT_EQ("224.3.3.3", configuration.multicastGroupIP(0));
  EXPECT_EQ(3333, configuration.portNumber(0));
}

/// @brief A named feed that was never given an address must not look configured.
///
/// The placeholder was built with -1 for a port, but the member is an unsigned
/// short, so the sentinel became 65535 -- a legal port, indistinguishable from
/// one the operator asked for. There was no representable "unset".
TEST(QuickFAST, testFeedWithoutAnAddressIsNotReportedAsConfigured)
{
  Application::DecoderConfiguration configuration;
  parse(configuration, {"-mname", "FEED_A", "-multicast", "224.1.1.1:1111",
                        "-mname", "FEED_B"});

  ASSERT_EQ(2u, configuration.multicastCount());
  EXPECT_TRUE(configuration.multicastGroupIP(1).empty());
  EXPECT_FALSE(configuration.portNumberIsSet(1));
  EXPECT_TRUE(configuration.portNumberIsSet(0));
}
