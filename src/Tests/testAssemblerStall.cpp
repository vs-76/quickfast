// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// StreamingAssembler took the block size from the header at face value. With
// waitForCompleteMessage set it then waited for that many bytes, and if the
// number was larger than the buffer pool can ever hold, needBytes could not
// satisfy it however long the wait went on. The header stayed consumed and
// headerIsComplete_ stayed set, so every later packet was counted towards a
// block that would never complete: one bad size field and the stream is dead
// with no diagnostic at all.
//
// Its call to analyzeHeader was also the only one in the library outside an
// error path, so a header analyzer that rejects its input took the receiver
// down with an escaping exception rather than reporting a decoding error.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/StreamingAssembler.h>
#include <Codecs/FastEncodedHeaderAnalyzer.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/XMLTemplateParser.h>
#include <Communication/BufferReceiver.h>
#include <Messages/ValueMessageBuilder.h>

using namespace QuickFAST;

namespace
{
  /// @brief A builder that records decoding errors instead of printing them.
  class RecordingBuilder : public Messages::ValueMessageBuilder
  {
  public:
    std::vector<std::string> errors_;

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

  /// @brief Encode a value as a FAST-encoded header block size.
  std::string sevenBitEncoded(uint64 value)
  {
    std::string reversed;
    reversed.push_back(static_cast<char>((value & 0x7f) | 0x80));
    value >>= 7;
    while(value != 0)
    {
      reversed.push_back(static_cast<char>(value & 0x7f));
      value >>= 7;
    }
    return std::string(reversed.rbegin(), reversed.rend());
  }

  /// @brief Feed one buffer through a waiting streaming assembler.
  /// @returns the errors the builder was told about.
  std::vector<std::string> feed(const std::string & data)
  {
    Codecs::FastEncodedHeaderAnalyzer analyzer(0, 0, true);
    RecordingBuilder builder;
    Codecs::StreamingAssembler assembler(trivialRegistry(), analyzer, builder, true);
    Communication::BufferReceiver receiver;
    // One buffer, because BufferReceiver holds a single external region: with
    // a second buffer in the pool, startReceive asks for data that is not
    // there and stops the receiver before the queue is ever serviced.
    receiver.start(assembler, 1500, 1);
    receiver.receiveBuffer(
      reinterpret_cast<const unsigned char *>(data.data()), data.size());
    return builder.errors_;
  }
}

/// @brief An unsatisfiable block size must be reported, not waited on.
TEST(QuickFAST, testUnsatisfiableBlockSizeIsReported)
{
  const std::vector<std::string> errors =
    feed(sevenBitEncoded(uint64(1) << 40) + std::string(16, '\x80'));
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(std::string::npos, errors.front().find("exceeds"));
}

/// @brief An overlong block size must be reported, not thrown out of the loop.
TEST(QuickFAST, testOverlongBlockSizeIsReportedNotThrown)
{
  std::string header(10, '\x01');
  header.push_back('\x81');
  const std::vector<std::string> errors = feed(header + std::string(16, '\x80'));
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(std::string::npos, errors.front().find("too large"));
}

/// @brief A block size the pool can hold is still waited for as before.
TEST(QuickFAST, testSatisfiableBlockSizeIsNotReported)
{
  // One message: a pmap with the template id bit, the template id, the value.
  const std::string body("\xC0\x81\x82", 3);
  const std::vector<std::string> errors =
    feed(sevenBitEncoded(body.size()) + body);
  EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
}
