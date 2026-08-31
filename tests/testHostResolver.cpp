// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Communication/HostResolver.h>

using QuickFAST::Communication::resolveTcp;

TEST(HostResolver, RejectsEmptyHost)
{
  asio::io_context io;
  asio::error_code ec;
  const auto endpoints = resolveTcp(io, "", "80", ec);
  EXPECT_TRUE(ec);
  EXPECT_TRUE(endpoints.empty());
}

TEST(HostResolver, ResolvesNumericIpv4AndPort)
{
  asio::io_context io;
  asio::error_code ec;
  const auto endpoints = resolveTcp(io, "127.0.0.1", "8080", ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_TRUE(endpoints[0].address().is_v4());
  EXPECT_EQ(asio::ip::make_address_v4("127.0.0.1"), endpoints[0].address().to_v4());
  EXPECT_EQ(8080, endpoints[0].port());
}

TEST(HostResolver, ResolvesNumericIpv6AndPort)
{
  asio::io_context io;
  asio::error_code ec;
  const auto endpoints = resolveTcp(io, "::1", "443", ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_FALSE(endpoints.empty());
  EXPECT_TRUE(endpoints[0].address().is_v6());
  EXPECT_EQ(443, endpoints[0].port());
}

TEST(HostResolver, ResolvesLocalhost)
{
  asio::io_context io;
  asio::error_code ec;
  const auto endpoints = resolveTcp(io, "localhost", "7", ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_FALSE(endpoints.empty());
  // AF_UNSPEC: at least one loopback family.
  bool loopback = false;
  for(const auto & endpoint : endpoints)
  {
    if(endpoint.address().is_loopback())
    {
      loopback = true;
    }
    EXPECT_EQ(7, endpoint.port());
  }
  EXPECT_TRUE(loopback);
}

TEST(HostResolver, ResolvesNamedServiceWithNumericHost)
{
  asio::io_context io;
  asio::error_code ec;
  // "echo" is TCP/7 in /etc/services on typical Unix hosts.
  const auto endpoints = resolveTcp(io, "127.0.0.1", "echo", ec);
  ASSERT_FALSE(ec) << ec.message();
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_EQ(7, endpoints[0].port());
  EXPECT_EQ(asio::ip::make_address_v4("127.0.0.1"), endpoints[0].address().to_v4());
}

TEST(HostResolver, RejectsUnknownHost)
{
  asio::io_context io;
  asio::error_code ec;
  const auto endpoints = resolveTcp(
    io,
    "no-such-host.quickfast.invalid",
    "80",
    ec);
  EXPECT_TRUE(ec);
  EXPECT_TRUE(endpoints.empty());
}
