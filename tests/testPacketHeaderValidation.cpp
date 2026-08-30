// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Four defects around packet framing:
//
//  - BasePacketAssembler parses the declared block size and never reads it
//    again, so a block that claims more bytes than the packet carries is not
//    caught at the framing layer at all;
//  - FixedSizeHeaderAnalyzer::getSequenceNumber takes no length, so it trusts
//    that the caller's buffer is long enough, and a sequence field wider than
//    a uint32 silently shifts its own leading bytes away;
//  - FastEncodedHeaderAnalyzer leaves both out-parameters untouched on every
//    incomplete-data return, unlike the other implementation of the same
//    interface;
//  - the received byte counter is incremented once too often per packet.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>
#include <Codecs/FastEncodedHeaderAnalyzer.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/BasePacketAssembler.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/XMLTemplateParser.h>
#include <Messages/ValueMessageBuilder.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  /// @brief A builder that records decoding errors instead of printing them.
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

  /// @brief Exposes decodeBuffer so a packet can be handed over directly.
  class TestAssembler : public Codecs::BasePacketAssembler
  {
  public:
    TestAssembler(
      Codecs::TemplateRegistryPtr registry,
      Codecs::HeaderAnalyzer & packetHeader,
      Codecs::HeaderAnalyzer & messageHeader,
      Messages::ValueMessageBuilder & builder)
      : BasePacketAssembler(registry, packetHeader, messageHeader, builder)
    {
    }

    using BasePacketAssembler::decodeBuffer;

    virtual void receiverStarted(Communication::Receiver &) {}
    virtual void receiverStopped(Communication::Receiver &) {}
    virtual bool serviceQueue(Communication::Receiver &)
    {
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
}

/// @brief A packet counts its own bytes once, not once plus one.
TEST(QuickFAST, testReceivedByteCountIsNotInflated)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  TestAssembler assembler(trivialRegistry(), packetHeader, messageHeader, builder);

  const std::string packet("\xC0\x81\x82", 3);
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());

  EXPECT_EQ(2u * packet.size(), assembler.byteCount());
}

/// @brief A block that claims more bytes than the packet holds is a framing
///        error, and the framing layer is where it should be caught.
TEST(QuickFAST, testOverlongDeclaredBlockIsReported)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(2, true);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  TestAssembler assembler(trivialRegistry(), packetHeader, messageHeader, builder);

  // Two byte big-endian block size of 200, then three bytes of message.
  const std::string packet("\x00\xC8\xC0\x81\x82", 5);
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());

  ASSERT_FALSE(builder.errors_.empty());
  EXPECT_NE(std::string::npos, builder.errors_.front().find("200"));
  EXPECT_EQ(0u, builder.messages_);
}

/// @brief A block whose declared size matches the packet still decodes.
TEST(QuickFAST, testDeclaredBlockSizeThatFitsStillDecodes)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(2, true);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  TestAssembler assembler(trivialRegistry(), packetHeader, messageHeader, builder);

  const std::string packet("\x00\x03\xC0\x81\x82", 5);
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());

  EXPECT_TRUE(builder.errors_.empty())
    << (builder.errors_.empty() ? "" : builder.errors_.front());
  EXPECT_EQ(1u, builder.messages_);
}

/// @brief A sequence field wider than its accumulator must be refused.
TEST(QuickFAST, testSequenceLengthWiderThanUInt32IsRejected)
{
  EXPECT_THROW(
    Codecs::FixedSizeHeaderAnalyzer(4, true, 0, 0, 0, 5),
    UsageError);
  EXPECT_NO_THROW(Codecs::FixedSizeHeaderAnalyzer(4, true, 0, 0, 0, 4));
}

/// @brief A buffer too short for the sequence field must be refused.
TEST(QuickFAST, testSequenceNumberChecksTheBufferLength)
{
  Codecs::FixedSizeHeaderAnalyzer analyzer(0, true, 0, 0, 4, 4);
  const uchar buffer[8] = {0, 0, 0, 0, 0x12, 0x34, 0x56, 0x78};
  EXPECT_EQ(0x12345678u, analyzer.getSequenceNumber(buffer, sizeof(buffer)));
  EXPECT_THROW((void)analyzer.getSequenceNumber(buffer, 7), UsageError);
}

/// @brief Little-endian sequence numbers still read as before.
TEST(QuickFAST, testSequenceNumberLittleEndian)
{
  Codecs::FixedSizeHeaderAnalyzer analyzer(0, false, 0, 0, 0, 4);
  const uchar buffer[4] = {0x78, 0x56, 0x34, 0x12};
  EXPECT_EQ(0x12345678u, analyzer.getSequenceNumber(buffer, sizeof(buffer)));
}

/// @brief Both out-parameters must be set before any early return.
TEST(QuickFAST, testFastEncodedHeaderInitialisesOutParameters)
{
  Codecs::FastEncodedHeaderAnalyzer analyzer(0, 0, true);
  // Two continuation bytes and then nothing: the parse cannot complete.
  Codecs::DataSourceString source(std::string("\x01\x01", 2));
  size_t blockSize = 0xDEADBEEF;
  bool skip = true;
  EXPECT_FALSE(analyzer.analyzeHeader(source, blockSize, skip));
  EXPECT_EQ(0u, blockSize);
  EXPECT_FALSE(skip);
}
