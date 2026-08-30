// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// ReceiveTime and PktSize are taken from the accepted buffer and injected as
// synthetic uInt64 fields after FAST decode (UTC µs since epoch, and byte size).
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <chrono>

#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/MessagePerPacketAssembler.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/XMLTemplateParser.h>
#include <Communication/BufferReceiver.h>
#include <Communication/LinkedBuffer.h>
#include <Messages/Field.h>
#include <Messages/FieldIdentity.h>
#include <Messages/Message.h>

using namespace QuickFAST;

namespace
{
  Codecs::TemplateRegistryPtr trivialRegistry()
  {
    std::stringstream templates(
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <uInt32 name=\"value\"/>"
      "  </template>"
      "</templates>");
    Codecs::XMLTemplateParser parser;
    return parser.parse(templates);
  }

  uint64 utcNowUs()
  {
    return static_cast<uint64>(
      std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }
}

TEST(QuickFAST, testLinkedBufferStampsReceiveTime)
{
  Communication::LinkedBuffer buffer(16);
  EXPECT_FALSE(buffer.hasReceiveTime());
  EXPECT_EQ(0u, buffer.receiveTime());

  const uint64 before = utcNowUs();
  buffer.stampReceiveTime();
  const uint64 after = utcNowUs();

  ASSERT_TRUE(buffer.hasReceiveTime());
  EXPECT_GE(buffer.receiveTime(), before);
  EXPECT_LE(buffer.receiveTime(), after);

  buffer.clearReceiveTime();
  EXPECT_FALSE(buffer.hasReceiveTime());

  buffer.setReceiveTime(123456789012345ull);
  ASSERT_TRUE(buffer.hasReceiveTime());
  EXPECT_EQ(123456789012345ull, buffer.receiveTime());
}

/// @brief Synch accept stamps the buffer; GenericMessageBuilder adds ReceiveTime and PktSize.
TEST(QuickFAST, testReceiveTimeReachesMessageConsumer)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  Codecs::MessagePerPacketAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder);

  const uint64 before = utcNowUs();
  Communication::BufferReceiver receiver;
  receiver.start(assembler, 1500, 1);
  // Presence map + template id 1 + uInt32 value 2
  const unsigned char packet[] = {0xC0, 0x81, 0x82};
  receiver.receiveBuffer(packet, sizeof(packet));
  const uint64 after = utcNowUs();

  Messages::Message & message = consumer.message();
  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("ReceiveTime", field));
  ASSERT_TRUE(field->isUnsignedInteger());
  const uint64 receiveTime = field->toUInt64();
  EXPECT_GE(receiveTime, before);
  EXPECT_LE(receiveTime, after);

  Messages::FieldCPtr pktSize;
  ASSERT_TRUE(message.getField("PktSize", pktSize));
  ASSERT_TRUE(pktSize->isUnsignedInteger());
  EXPECT_EQ(sizeof(packet), pktSize->toUInt64());

  // Template field is still present.
  Messages::FieldCPtr value;
  ASSERT_TRUE(message.getField("value", value));
  EXPECT_EQ(2u, value->toUInt32());
}

/// @brief A template field named ReceiveTime is left alone.
TEST(QuickFAST, testReceiveTimeDoesNotOverwriteTemplateField)
{
  std::stringstream templates(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <uInt64 name=\"ReceiveTime\"/>"
    "  </template>"
    "</templates>");
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr registry = parser.parse(templates);

  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  Codecs::MessagePerPacketAssembler assembler(
    registry, packetHeader, messageHeader, builder);

  Communication::BufferReceiver receiver;
  receiver.start(assembler, 1500, 1);
  // Presence map + template id 1 + uInt64 value 7 (stop bit set)
  const unsigned char packet[] = {0xC0, 0x81, 0x87};
  receiver.receiveBuffer(packet, sizeof(packet));

  Messages::Message & message = consumer.message();
  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("ReceiveTime", field));
  EXPECT_EQ(7u, field->toUInt64());

  Messages::FieldCPtr pktSize;
  ASSERT_TRUE(message.getField("PktSize", pktSize));
  EXPECT_EQ(sizeof(packet), pktSize->toUInt64());
}

/// @brief A template field named PktSize is left alone.
TEST(QuickFAST, testPktSizeDoesNotOverwriteTemplateField)
{
  std::stringstream templates(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <uInt64 name=\"PktSize\"/>"
    "  </template>"
    "</templates>");
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr registry = parser.parse(templates);

  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  Codecs::MessagePerPacketAssembler assembler(
    registry, packetHeader, messageHeader, builder);

  Communication::BufferReceiver receiver;
  receiver.start(assembler, 1500, 1);
  const unsigned char packet[] = {0xC0, 0x81, 0x87};
  receiver.receiveBuffer(packet, sizeof(packet));

  Messages::Message & message = consumer.message();
  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("PktSize", field));
  EXPECT_EQ(7u, field->toUInt64());
}
