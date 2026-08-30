// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Second pass on product-critical gaps: dynamic templateRef (Decoder nested
// path), dictionary indexing scopes, FAST header prefix/suffix incompleteness,
// BasePacketAssembler / StreamingAssembler skip and error edges, PresenceMap
// capacity growth, and optional groups with a distinct typeRef.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/FastEncodedHeaderAnalyzer.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/BasePacketAssembler.h>
#include <Codecs/StreamingAssembler.h>
#include <Codecs/PresenceMap.h>
#include <Codecs/HeaderAnalyzer.h>
#include <Communication/BufferReceiver.h>
#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldGroup.h>
#include <Messages/FieldSet.h>
#include <Messages/ValueMessageBuilder.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  Codecs::TemplateRegistryPtr parse(const std::string & xml)
  {
    std::stringstream stream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(stream);
  }

  class RecordingBuilder : public Messages::ValueMessageBuilder
  {
  public:
    explicit RecordingBuilder(bool logInfo = false)
      : logInfo_(logInfo)
    {
    }

    std::vector<std::string> errors_;
    std::vector<std::string> logs_;
    size_t messages_ = 0;
    size_t groups_ = 0;
    bool logInfo_;

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
      ++groups_;
      return *this;
    }
    virtual void endGroup(const Messages::FieldIdentity &, ValueMessageBuilder &) {}
    virtual bool wantLog(unsigned short level)
    {
      return logInfo_ && level <= Common::Logger::QF_LOG_INFO;
    }
    virtual bool logMessage(unsigned short, const std::string & message)
    {
      logs_.push_back(message);
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
    virtual bool serviceQueue(Communication::Receiver &) { return false; }
  };

  class SkipMessageHeader : public Codecs::HeaderAnalyzer
  {
  public:
    virtual bool analyzeHeader(Codecs::DataSource &, size_t & blockSize, bool & skip)
    {
      blockSize = 0;
      skip = true;
      return true;
    }
  };

  class SkipBlockHeader : public Codecs::HeaderAnalyzer
  {
  public:
    virtual bool analyzeHeader(Codecs::DataSource & source, size_t & blockSize, bool & skip)
    {
      // Consume nothing; declare a zero-size skipped block.
      blockSize = 0;
      skip = true;
      (void)source;
      return true;
    }
  };

  class IncompleteThenCompleteHeader : public Codecs::HeaderAnalyzer
  {
  public:
    int calls_ = 0;
    virtual bool analyzeHeader(Codecs::DataSource &, size_t & blockSize, bool & skip)
    {
      blockSize = 0;
      skip = false;
      ++calls_;
      // First call incomplete, later calls complete with no block size.
      return calls_ > 1;
    }
  };

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
}

/// @brief A dynamic templateRef must decode the nested template into a group.
TEST(QuickFAST, testDynamicTemplateRefDecodesNestedTemplate)
{
  Codecs::TemplateRegistryPtr registry = parse(
    "<templates>"
    "  <template name=\"outer\" id=\"1\">"
    "    <templateRef/>"
    "  </template>"
    "  <template name=\"inner\" id=\"2\">"
    "    <typeRef name=\"Inner\"/>"
    "    <uInt32 name=\"value\"/>"
    "  </template>"
    "</templates>");

  // Outer tid=1, then nested pmap+tid=2+value=1.
  const std::string wire("\xC0\x81\xC0\x82\x81", 5);
  Codecs::DataSourceString source(wire);
  Codecs::Decoder decoder(registry);
  RecordingBuilder builder;
  decoder.decodeMessage(source, builder);

  EXPECT_EQ(1u, builder.messages_);
  EXPECT_EQ(1u, builder.groups_);
}

/// @brief Encoding a dynamic templateRef is intentionally unsupported.
TEST(QuickFAST, testDynamicTemplateRefEncodeIsRejected)
{
  Codecs::TemplateRegistryPtr registry = parse(
    "<templates>"
    "  <template name=\"outer\" id=\"1\">"
    "    <templateRef/>"
    "  </template>"
    "  <template name=\"inner\" id=\"2\">"
    "    <uInt32 name=\"value\"/>"
    "  </template>"
    "</templates>");

  Messages::Message message(registry->maxFieldCount());
  Codecs::Encoder encoder(registry);
  Codecs::DataDestination destination;
  EXPECT_THROW(
    encoder.encodeMessage(destination, 1, message),
    EncodingError);
}

/// @brief An unknown static templateRef name must fail when the registry finalizes.
TEST(QuickFAST, testUnknownStaticTemplateRefIsRejected)
{
  EXPECT_THROW(
    (void)parse(
      "<templates>"
      "  <template name=\"outer\" id=\"1\">"
      "    <templateRef name=\"missing\"/>"
      "  </template>"
      "</templates>"),
    TemplateDefinitionError);
}

/// @brief dictionary= type/template/named/global must all index without clash.
TEST(QuickFAST, testDictionaryScopesIndexIndependently)
{
  Codecs::TemplateRegistryPtr registry = parse(
    "<templates dictionary=\"session\">"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"T\"/>"
    "    <uInt32 name=\"a\"><copy dictionary=\"type\"/></uInt32>"
    "    <uInt32 name=\"b\"><copy dictionary=\"template\"/></uInt32>"
    "    <uInt32 name=\"c\"><copy dictionary=\"global\"/></uInt32>"
    "    <uInt32 name=\"d\"><copy dictionary=\"custom\"/></uInt32>"
    "    <uInt32 name=\"e\"><copy/></uInt32>"
    "  </template>"
    "</templates>");

  EXPECT_GE(registry->dictionarySize(), 5u);

  const Messages::FieldIdentity a("a");
  const Messages::FieldIdentity b("b");
  const Messages::FieldIdentity c("c");
  const Messages::FieldIdentity d("d");
  const Messages::FieldIdentity e("e");

  Messages::Message message(registry->maxFieldCount());
  message.addField(a, Messages::FieldUInt32::create(1));
  message.addField(b, Messages::FieldUInt32::create(2));
  message.addField(c, Messages::FieldUInt32::create(3));
  message.addField(d, Messages::FieldUInt32::create(4));
  message.addField(e, Messages::FieldUInt32::create(5));

  Codecs::Encoder encoder(registry);
  Codecs::DataDestination destination;
  encoder.encodeMessage(destination, 1, message);
  std::string fast;
  destination.toString(fast);

  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  decoder.decodeMessage(source, builder);

  Messages::FieldCPtr field;
  ASSERT_TRUE(consumer.message().getField("e", field));
  EXPECT_EQ(5u, field->toUInt32());
}

/// @brief Prefix/suffix fields and missing bytes must return incomplete, not throw.
TEST(QuickFAST, testFastEncodedHeaderPrefixSuffixAndIncomplete)
{
  // One prefix field (stop byte), block size 3, one suffix field.
  {
    Codecs::FastEncodedHeaderAnalyzer analyzer(1, 1, true);
    Codecs::DataSourceString source(std::string("\x80", 1)); // only prefix
    size_t blockSize = 99;
    bool skip = true;
    EXPECT_FALSE(analyzer.analyzeHeader(source, blockSize, skip));
    EXPECT_EQ(0u, blockSize);
    EXPECT_FALSE(skip);
  }
  {
    Codecs::FastEncodedHeaderAnalyzer analyzer(1, 1, true);
    const std::string header =
      std::string("\x80", 1) + sevenBitEncoded(3) + std::string("\x80", 1);
    Codecs::DataSourceString source(header);
    size_t blockSize = 0;
    bool skip = true;
    ASSERT_TRUE(analyzer.analyzeHeader(source, blockSize, skip));
    EXPECT_EQ(3u, blockSize);
    EXPECT_FALSE(skip);
  }
}

/// @brief A header without a block size still completes when prefix/suffix are done.
TEST(QuickFAST, testFastEncodedHeaderWithoutBlockSize)
{
  Codecs::FastEncodedHeaderAnalyzer analyzer(1, 1, false);
  const std::string header("\x80\x80", 2);
  Codecs::DataSourceString source(header);
  size_t blockSize = 1;
  bool skip = true;
  ASSERT_TRUE(analyzer.analyzeHeader(source, blockSize, skip));
  EXPECT_EQ(0u, blockSize);
  EXPECT_FALSE(skip);
}

/// @brief An overlong message header size is reported at the packet layer.
TEST(QuickFAST, testBasePacketAssemblerRejectsOverlongMessageHeader)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::FixedSizeHeaderAnalyzer messageHeader(2, true);
  RecordingBuilder builder;
  TestAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    packetHeader, messageHeader, builder);

  // Message header claims 200 bytes; only three follow.
  const std::string packet("\x00\xC8\xC0\x81\x82", 5);
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());
  ASSERT_FALSE(builder.errors_.empty());
  EXPECT_NE(std::string::npos, builder.errors_.front().find("200"));
}

/// @brief A message header that asks to skip must not decode the remainder.
TEST(QuickFAST, testBasePacketAssemblerHonoursSkipMessage)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  SkipMessageHeader messageHeader;
  RecordingBuilder builder;
  TestAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    packetHeader, messageHeader, builder);

  const std::string packet("\xC0\x81\x82", 3);
  assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());
  EXPECT_EQ(0u, builder.messages_);
}

/// @brief receiverStarted/Stopped honour info logging when the builder wants it.
TEST(QuickFAST, testBasePacketAssemblerLogsStartAndStop)
{
  Codecs::NoHeaderAnalyzer packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder(true);
  TestAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    packetHeader, messageHeader, builder);

  Communication::BufferReceiver receiver;
  assembler.receiverStarted(receiver);
  assembler.receiverStopped(receiver);
  ASSERT_GE(builder.logs_.size(), 2u);
  EXPECT_NE(std::string::npos, builder.logs_[0].find("started"));
  EXPECT_NE(std::string::npos, builder.logs_[1].find("stopped"));
}

/// @brief skipBlock is accepted without hanging; decode still runs (skip not implemented).
///
/// StreamingAssembler clears skipBlock_ but the "//not implemented yet" skip
/// path never suppresses decodeMessage. Pin that so a future implementation
/// cannot land silently without updating this test.
TEST(QuickFAST, testStreamingAssemblerSkipBlockDoesNotHang)
{
  SkipBlockHeader header;
  RecordingBuilder builder;
  Codecs::StreamingAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    header, builder, false);

  Communication::BufferReceiver receiver;
  receiver.start(assembler, 1500, 1);
  const std::string packet("\xC0\x81\x82", 3);
  receiver.receiveBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size());
  EXPECT_EQ(1u, builder.messages_);
}

/// @brief A decode error that the builder rejects must stop the streaming loop.
TEST(QuickFAST, testStreamingAssemblerStopsOnRejectedDecodeError)
{
  Codecs::NoHeaderAnalyzer header;
  RecordingBuilder builder;
  Codecs::StreamingAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    header, builder, false);

  Communication::BufferReceiver receiver;
  receiver.start(assembler, 1500, 1);
  // Complete message naming unknown template id 99 — throws without needing
  // another buffer (which would hang BufferReceiver in waitBuffer).
  const std::string data("\xC0\xE3\x81", 3);
  receiver.receiveBuffer(
    reinterpret_cast<const unsigned char *>(data.data()), data.size());
  ASSERT_FALSE(builder.errors_.empty());
}

/// @brief StreamingAssembler start/stop log when the builder wants INFO.
TEST(QuickFAST, testStreamingAssemblerLogsStartAndStop)
{
  Codecs::NoHeaderAnalyzer header;
  RecordingBuilder builder(true);
  Codecs::StreamingAssembler assembler(
    parse("<templates><template name=\"t\" id=\"1\">"
          "<uInt32 name=\"value\"/></template></templates>"),
    header, builder, false);

  Communication::BufferReceiver receiver;
  assembler.receiverStarted(receiver);
  assembler.receiverStopped(receiver);
  ASSERT_GE(builder.logs_.size(), 2u);
}

/// @brief Optional group with its own typeRef becomes a nested group field.
TEST(QuickFAST, testOptionalGroupWithTypeRefRoundTrips)
{
  Codecs::TemplateRegistryPtr registry = parse(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"Parent\"/>"
    "    <group name=\"g\" presence=\"optional\">"
    "      <typeRef name=\"Child\"/>"
    "      <uInt32 name=\"value\"/>"
    "    </group>"
    "  </template>"
    "</templates>");

  const Messages::FieldIdentity groupId("g");
  const Messages::FieldIdentity valueId("value");
  Messages::FieldSetPtr child(new Messages::FieldSet(1));
  child->setApplicationType("Child", "");
  child->addField(valueId, Messages::FieldUInt32::create(7));

  Messages::Message message(registry->maxFieldCount());
  message.setApplicationType("Parent", "");
  message.addField(groupId, Messages::FieldGroup::create(Messages::GroupCPtr(child)));

  Codecs::Encoder encoder(registry);
  Codecs::DataDestination destination;
  encoder.encodeMessage(destination, 1, message);
  std::string fast;
  destination.toString(fast);

  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  decoder.decodeMessage(source, builder);

  Messages::FieldCPtr field;
  ASSERT_TRUE(consumer.message().getField("g", field));
  Messages::GroupCPtr group = field->toGroup();
  ASSERT_TRUE(group->getField("value", field));
  EXPECT_EQ(7u, field->toUInt32());
}

/// @brief Optional group absent must not invent a group.
TEST(QuickFAST, testOptionalGroupAbsentStaysAbsent)
{
  Codecs::TemplateRegistryPtr registry = parse(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"Parent\"/>"
    "    <group name=\"g\" presence=\"optional\">"
    "      <typeRef name=\"Child\"/>"
    "      <uInt32 name=\"value\"/>"
    "    </group>"
    "  </template>"
    "</templates>");

  Messages::Message message(registry->maxFieldCount());
  message.setApplicationType("Parent", "");

  Codecs::Encoder encoder(registry);
  Codecs::DataDestination destination;
  encoder.encodeMessage(destination, 1, message);
  std::string fast;
  destination.toString(fast);

  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  decoder.decodeMessage(source, builder);

  Messages::FieldCPtr field;
  EXPECT_FALSE(consumer.message().getField("g", field));
}

/// @brief PresenceMap setRaw/reset must grow capacity and clear prior bits.
TEST(QuickFAST, testPresenceMapGrowsOnSetRawAndReset)
{
  Codecs::PresenceMap small(1);
  std::vector<uchar> wide(40, 0x7f);
  wide.back() = 0xff;
  small.setRaw(wide.data(), wide.size());

  const uchar * raw = 0;
  size_t capacity = 0;
  small.getRaw(raw, capacity);
  EXPECT_GE(capacity, wide.size());

  Codecs::PresenceMap resized(1);
  resized.reset(400);
  resized.getRaw(raw, capacity);
  // reset() sizes by (bitCount+7)/8 — pin the growth, not the formula's ideal.
  EXPECT_GE(capacity, 50u);
  for(size_t bit = 0; bit < 50; ++bit)
  {
    EXPECT_FALSE(resized.checkNextField());
  }
}

/// @brief An unused presence map encodes as nothing.
TEST(QuickFAST, testPresenceMapUnusedEncodesNothing)
{
  Codecs::PresenceMap pmap(10);
  EXPECT_EQ(0u, pmap.encodeBytesNeeded());
  Codecs::DataDestination destination;
  pmap.encode(destination);
  destination.endMessage();
  std::string wire;
  destination.toString(wire);
  EXPECT_TRUE(wire.empty());
}
