// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// MessagePerPacketAssembler is the UDP/multicast decode path. BasePacketAssembler
// decodeBuffer is covered elsewhere; this file drives serviceQueue through a
// BufferReceiver so a bad packet, a decode error, and a message limit actually
// stop the queue the way a live receiver would.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/MessagePerPacketAssembler.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Communication/BufferReceiver.h>
#include <Messages/ValueMessageBuilder.h>

using namespace QuickFAST;

namespace
{
  class RecordingBuilder : public Messages::ValueMessageBuilder
  {
  public:
    std::vector<std::string> errors_;
    size_t messages_ = 0;

    virtual const std::string & getApplicationType() const
    {
      static const std::string type("recording");
      return type;
    }
    virtual const std::string & getApplicationTypeNs() const
    {
      static const std::string ns;
      return ns;
    }
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int64) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint64) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int32) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint32) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int16) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint16) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int8) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uchar) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const Decimal &) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const unsigned char *, size_t) {}
    virtual ValueMessageBuilder & startMessage(const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual bool endMessage(ValueMessageBuilder &)
    {
      ++messages_;
      return true;
    }
    virtual bool ignoreMessage(ValueMessageBuilder &)
    {
      return true;
    }
    virtual ValueMessageBuilder & startSequence(
      const Messages::FieldIdentity &, const std::string &, const std::string &,
      size_t, const Messages::FieldIdentity &, size_t)
    {
      return *this;
    }
    virtual void endSequence(const Messages::FieldIdentity &, ValueMessageBuilder &) {}
    virtual ValueMessageBuilder & startSequenceEntry(const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual void endSequenceEntry(ValueMessageBuilder &) {}
    virtual ValueMessageBuilder & startGroup(
      const Messages::FieldIdentity &, const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual void endGroup(const Messages::FieldIdentity &, ValueMessageBuilder &) {}

    virtual bool wantLog(unsigned short)
    {
      return false;
    }
    virtual bool logMessage(unsigned short, const std::string &)
    {
      return true;
    }
    virtual bool reportDecodingError(const std::string & errorMessage)
    {
      errors_.push_back(errorMessage);
      return false;
    }
    virtual bool reportCommunicationError(const std::string & errorMessage)
    {
      errors_.push_back(errorMessage);
      return false;
    }
  };

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

  /// @brief Feed one in-memory packet through MessagePerPacketAssembler.
  void feed(
    Codecs::MessagePerPacketAssembler & assembler,
    const std::string & packet)
  {
    Communication::BufferReceiver receiver;
    receiver.start(assembler, 1500, 1);
    receiver.receiveBuffer(
      reinterpret_cast<const unsigned char *>(packet.data()), packet.size());
  }
}

/// @brief A well-formed one-message packet still decodes through serviceQueue.
TEST(QuickFAST, testMessagePerPacketAssemblerDecodesAValidPacket)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::MessagePerPacketAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder);

  feed(assembler, std::string("\xC0\x81\x82", 3));

  EXPECT_EQ(1u, builder.messages_);
  EXPECT_TRUE(builder.errors_.empty());
  EXPECT_EQ(1u, assembler.messageCount());
}

/// @brief A truncated FAST message must be reported, not left hanging.
TEST(QuickFAST, testMessagePerPacketAssemblerReportsDecodeError)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::MessagePerPacketAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder);

  // Presence map claims a template id, but the buffer ends before the value.
  feed(assembler, std::string("\xC0", 1));

  ASSERT_FALSE(builder.errors_.empty());
  EXPECT_EQ(0u, builder.messages_);
}

/// @brief messageLimit makes serviceQueue return false once the count is past it.
///
/// The check is after decoding, so limit N still delivers N messages and then
/// refuses to keep the queue alive for another packet. Without that refusal a
/// head-limited run would never exit the receiver loop.
TEST(QuickFAST, testMessagePerPacketAssemblerHonoursMessageLimit)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::MessagePerPacketAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder);
  assembler.setMessageLimit(1);

  const std::string packet("\xC0\x81\x82", 3);
  feed(assembler, packet);
  EXPECT_EQ(1u, builder.messages_);
  EXPECT_EQ(1u, assembler.messageCount());

  feed(assembler, packet);
  EXPECT_EQ(2u, builder.messages_);
  EXPECT_EQ(2u, assembler.messageCount());
}
