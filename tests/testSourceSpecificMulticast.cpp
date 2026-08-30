// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <stdexcept>

#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/XMLTemplateParser.h>
#include <Common/Exceptions.h>
#include <Communication/MulticastReceiver.h>
#include <Communication/SourceSpecificMulticastReceiver.h>

using namespace QuickFAST;
using namespace QuickFAST::Communication;

namespace
{
  /// A feed needs a host to hand buffers back to; these tests never read.
  class NullFeedHost : public MulticastFeedHost
  {
  public:
    virtual void feedReceived(const asio::error_code &, LinkedBuffer *, size_t)
    {
    }

    virtual bool feedStopping() const
    {
      return false;
    }
  };

  /// acceptSender() is protected because only the read loop should call it.
  class ExposedFeed : public SourceSpecificMulticastFeed
  {
  public:
    ExposedFeed(
      MulticastFeedHost & host,
      AsioService & ioService,
      const std::string & group,
      const SourceList & sources,
      const std::string & listenInterface)
      : SourceSpecificMulticastFeed(
          host, ioService, "test", group, sources, listenInterface, group, 13000)
    {
    }

    bool accepts(const std::string & senderIP) const
    {
      return acceptSender(
        asio::ip::udp::endpoint(asio::ip::make_address(senderIP), 13000));
    }
  };

  /// feed() is protected because feeds are an implementation detail.
  class ExposedReceiver : public SourceSpecificMulticastReceiver
  {
  public:
    using SourceSpecificMulticastReceiver::feed;
  };

  SourceSpecificMulticastFeed::SourceList sources(
    const std::string & first,
    const std::string & second = std::string())
  {
    SourceSpecificMulticastFeed::SourceList list;
    list.push_back(first);
    if(!second.empty())
    {
      list.push_back(second);
    }
    return list;
  }

  const char * const theTemplate =
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"t\"/>"
    "    <uInt32 name=\"value\"><nop/></uInt32>"
    "  </template>"
    "</templates>";

  void configure(
    Application::DecoderConnection & connection,
    Application::DecoderConfiguration & configuration)
  {
    std::stringstream templateStream{std::string(theTemplate)};
    Codecs::XMLTemplateParser parser;
    connection.setTemplateRegistry(parser.parse(templateStream));

    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    connection.configure(builder, configuration);
  }

  /// Configuring also starts the receiver, and joining a group needs a host
  /// with a multicast route -- which a build container may well not have.
  /// The receiver has been created and stored by the time the join is
  /// attempted, so which receiver the configuration asked for is still
  /// answerable when the join fails.
  void configureIgnoringStartupFailure(
    Application::DecoderConnection & connection,
    Application::DecoderConfiguration & configuration)
  {
    try
    {
      configure(connection, configuration);
    }
    catch(const UsageError &)
    {
    }
  }

  Application::DecoderConfiguration multicastConfiguration()
  {
    Application::DecoderConfiguration configuration;
    configuration.setReceiverType(
      Application::DecoderConfiguration::MULTICAST_RECEIVER);
    configuration.setAssemblerType(
      Application::DecoderConfiguration::MESSAGE_PER_PACKET_ASSEMBLER);
    configuration.setMulticastGroupIP("232.1.1.1");
    configuration.setPortNumber(13000);
    configuration.setListenInterfaceIP("0.0.0.0");
    return configuration;
  }
}

#if QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST

TEST(QuickFAST, testSourceMembershipOptionCarriesTheThreeAddresses)
{
  // The layout of ip_mreq_source differs between Linux and Windows/BSD, so the
  // option is worth checking field by field rather than trusting the order the
  // constructor arguments happen to be written in.
  const MulticastSource::join_source_group option(
    asio::ip::make_address_v4("232.1.2.3"),
    asio::ip::make_address_v4("10.20.30.40"),
    asio::ip::make_address_v4("192.168.1.1"));

  const asio::ip::udp protocol = asio::ip::udp::v4();
  ASSERT_EQ((sizeof(::ip_mreq_source)), (option.size(protocol)));
  EXPECT_EQ((IPPROTO_IP), (option.level(protocol)));
  EXPECT_EQ((IP_ADD_SOURCE_MEMBERSHIP), (option.name(protocol)));

  ::ip_mreq_source request;
  std::memcpy(&request, option.data(protocol), sizeof(request));
  EXPECT_EQ((::htonl(0xE8010203u)), (request.imr_multiaddr.s_addr));
  EXPECT_EQ((::htonl(0x0A141E28u)), (request.imr_sourceaddr.s_addr));
  EXPECT_EQ((::htonl(0xC0A80101u)), (request.imr_interface.s_addr));
}

TEST(QuickFAST, testSourceMembershipOptionNamesTheDropRequest)
{
  const MulticastSource::leave_source_group option(
    asio::ip::make_address_v4("232.1.2.3"),
    asio::ip::make_address_v4("10.20.30.40"),
    asio::ip::make_address_v4("0.0.0.0"));

  EXPECT_EQ((IP_DROP_SOURCE_MEMBERSHIP), (option.name(asio::ip::udp::v4())));
}

#endif // QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST

TEST(QuickFAST, testSourceSpecificRangeIsRecognized)
{
  EXPECT_TRUE(MulticastSource::isSourceSpecificRange(asio::ip::make_address_v4("232.0.0.0")));
  EXPECT_TRUE(MulticastSource::isSourceSpecificRange(asio::ip::make_address_v4("232.255.255.255")));
  EXPECT_FALSE(MulticastSource::isSourceSpecificRange(asio::ip::make_address_v4("231.255.255.255")));
  EXPECT_FALSE(MulticastSource::isSourceSpecificRange(asio::ip::make_address_v4("233.0.0.1")));
  EXPECT_FALSE(MulticastSource::isSourceSpecificRange(asio::ip::make_address_v4("224.1.1.1")));
}

TEST(QuickFAST, testSourceSpecificFeedKeepsItsSourceList)
{
  NullFeedHost host;
  AsioService ioService;
  ExposedFeed feed(host, ioService, "232.1.1.1", sources("10.0.0.1", "10.0.0.2"), "0.0.0.0");

  ASSERT_EQ((2u), (feed.sources().size()));
  EXPECT_EQ((asio::ip::make_address_v4("10.0.0.1")), (feed.sources()[0]));
  EXPECT_EQ((asio::ip::make_address_v4("10.0.0.2")), (feed.sources()[1]));
  EXPECT_FALSE(feed.joined());
  EXPECT_TRUE(feed.canStartRead());
}

TEST(QuickFAST, testSourceSpecificFeedFiltersBySender)
{
  // The kernel is asked to filter, but a network that falls back to IGMPv2
  // delivers the whole group; the feed must not pass those packets on.
  NullFeedHost host;
  AsioService ioService;
  ExposedFeed feed(host, ioService, "232.1.1.1", sources("10.0.0.1", "10.0.0.2"), "0.0.0.0");

  EXPECT_TRUE(feed.accepts("10.0.0.1"));
  EXPECT_TRUE(feed.accepts("10.0.0.2"));
  EXPECT_FALSE(feed.accepts("10.0.0.3"));
  EXPECT_FALSE(feed.accepts("::1"));
  EXPECT_EQ((0u), (feed.rejectedPackets()));
}

TEST(QuickFAST, testSourceSpecificFeedRejectsAnEmptySourceList)
{
  NullFeedHost host;
  AsioService ioService;
  const SourceSpecificMulticastFeed::SourceList empty;

  EXPECT_THROW(
    (ExposedFeed(host, ioService, "232.1.1.1", empty, "0.0.0.0")),
    std::invalid_argument);
}

TEST(QuickFAST, testSourceSpecificFeedRejectsNonV4Addresses)
{
  NullFeedHost host;
  AsioService ioService;

  EXPECT_THROW(
    (ExposedFeed(host, ioService, "ff3e::1234", sources("10.0.0.1"), "0.0.0.0")),
    std::invalid_argument);
  EXPECT_THROW(
    (ExposedFeed(host, ioService, "232.1.1.1", sources("fe80::1"), "0.0.0.0")),
    std::invalid_argument);
}

TEST(QuickFAST, testSourceSpecificReceiverBindsToTheGroupByDefault)
{
  // An empty bind address means the group address, not the interface address:
  // binding a source specific subscription to the unicast address of the NIC
  // makes some stacks drop the group traffic entirely.
  ExposedReceiver receiver;
  receiver.addFeed("A", "232.1.1.1", sources("10.0.0.1"), "0.0.0.0", "", 13000);
  receiver.addFeed("B", "232.1.1.2", sources("10.0.0.2"), "0.0.0.0", "0.0.0.0", 13001);

  ASSERT_EQ((2u), (receiver.feedCount()));
  EXPECT_EQ((asio::ip::make_address("232.1.1.1")), (receiver.feed(0).bindAddress()));
  EXPECT_EQ((asio::ip::make_address("0.0.0.0")), (receiver.feed(1).bindAddress()));
  EXPECT_EQ((0u), (receiver.rejectedPackets()));
}

TEST(QuickFAST, testSourceSpecificReceiverConvenienceConstructorAddsOneFeed)
{
  SourceSpecificMulticastReceiver receiver("232.1.1.1", "10.0.0.1", "0.0.0.0", "", 13000);

  EXPECT_EQ((1u), (receiver.feedCount()));
  EXPECT_EQ(
    (QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST != 0),
    (SourceSpecificMulticastFeed::kernelSourceFilter()));
}

TEST(QuickFAST, testConfiguredSourcesSelectTheSourceSpecificReceiver)
{
  Application::DecoderConfiguration configuration = multicastConfiguration();
  configuration.addMulticastSourceIP("10.0.0.1");

  Application::DecoderConnection connection;
  configureIgnoringStartupFailure(connection, configuration);

  EXPECT_NE(
    (dynamic_cast<SourceSpecificMulticastReceiver *>(&connection.receiver())),
    (static_cast<SourceSpecificMulticastReceiver *>(0)));
}

TEST(QuickFAST, testMulticastWithoutSourcesStaysAnySource)
{
  Application::DecoderConfiguration configuration = multicastConfiguration();

  Application::DecoderConnection connection;
  configureIgnoringStartupFailure(connection, configuration);

  EXPECT_NE(
    (dynamic_cast<MulticastReceiver *>(&connection.receiver())),
    (static_cast<MulticastReceiver *>(0)));
  EXPECT_EQ(
    (dynamic_cast<SourceSpecificMulticastReceiver *>(&connection.receiver())),
    (static_cast<SourceSpecificMulticastReceiver *>(0)));
}

TEST(QuickFAST, testHalfConfiguredSourcesAreRejected)
{
  // -msource applies to the current feed, so it is easy to give it to one feed
  // and mean it for both.  Silently leaving the other any-source would accept
  // the senders the operator asked to exclude.
  Application::DecoderConfiguration configuration = multicastConfiguration();
  configuration.setMulticastName("FEED_A");
  configuration.addMulticastSourceIP("10.0.0.1");
  configuration.setMulticastName("FEED_B");
  configuration.setMulticastGroupIP("232.2.2.2");
  configuration.setPortNumber(13001);

  Application::DecoderConnection connection;
  std::string reported;
  try
  {
    configure(connection, configuration);
    FAIL() << "a half configured source list must be rejected";
  }
  catch(const std::invalid_argument & exception)
  {
    reported = exception.what();
  }
  // A failed join reports through the same exception type, so the message is
  // what says the configuration was rejected for the right reason.
  EXPECT_NE((reported.find("names no source")), (std::string::npos)) << reported;
}
