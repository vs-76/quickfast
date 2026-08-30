// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Drive product-critical modules that still had large behavioural gaps:
// DataSource echo, Decoder unknown/SCP/ignore, Context logging, FieldOp names,
// PresenceMap verbose encode, Template display, SingleMessageConsumer, and a
// BasePacketAssembler skip path.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Template.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/FieldOp.h>
#include <Codecs/PresenceMap.h>
#include <Codecs/BasePacketAssembler.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/HeaderAnalyzer.h>
#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldUInt32.h>
#include <Messages/ValueMessageBuilder.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr parseTemplates(const std::string & xml)
  {
    std::stringstream stream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(stream);
  }

  std::string encodeUInt32(
    const Codecs::TemplateRegistryPtr & registry,
    template_id_t id,
    uint32 value)
  {
    Messages::Message message(registry->maxFieldCount());
    message.addField(valueIdentity, Messages::FieldUInt32::create(value));
    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, id, message);
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  class RecordingBuilder : public Messages::ValueMessageBuilder
  {
  public:
    std::vector<std::string> errors_;
    size_t messages_ = 0;
    size_t ignored_ = 0;

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
      ++ignored_;
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
    virtual bool wantLog(unsigned short) { return false; }
    virtual bool logMessage(unsigned short, const std::string &) { return true; }
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

  class SkippingPacketHeader : public Codecs::HeaderAnalyzer
  {
  public:
    virtual bool analyzeHeader(Codecs::DataSource &, size_t & blockSize, bool & skip)
    {
      blockSize = 0;
      skip = true;
      return true;
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
    virtual void receiverStarted(Communication::Receiver &) {}
    virtual void receiverStopped(Communication::Receiver &) {}
    virtual bool serviceQueue(Communication::Receiver &) { return false; }
  };
}

/// @brief HEX / RAW / NONE echo must not corrupt the decoded value.
TEST(QuickFAST, testDataSourceEchoModesPreserveDecode)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\">"
    "<uInt32 name=\"value\"/></template></templates>");
  const std::string fast = encodeUInt32(registry, 1, 99u);

  for(auto echoType : {Codecs::DataSource::HEX, Codecs::DataSource::RAW, Codecs::DataSource::NONE})
  {
    SCOPED_TRACE(static_cast<int>(echoType));
    std::ostringstream echo;
    Codecs::DataSourceString source(fast);
    source.setEcho(echo, echoType, true, true);
    EXPECT_GT(source.messageAvailable(), 0);

    Codecs::Decoder decoder(registry);
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    decoder.decodeMessage(source, builder);

    Messages::FieldCPtr field;
    ASSERT_TRUE(consumer.message().getField("value", field));
    EXPECT_EQ(99u, field->toUInt32());
    if(echoType != Codecs::DataSource::NONE)
    {
      EXPECT_FALSE(echo.str().empty());
    }
  }
}

/// @brief An unknown template id must be reported as [ERR D9].
TEST(QuickFAST, testDecoderRejectsUnknownTemplateId)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\">"
    "<uInt32 name=\"value\"/></template></templates>");

  // Presence map with template-id bit set, then template id 99, then a value.
  const std::string wire("\xC0\xE3\x81", 3);
  Codecs::DataSourceString source(wire);
  Codecs::Decoder decoder(registry);
  RecordingBuilder builder;
  EXPECT_THROW(decoder.decodeMessage(source, builder), EncodingError);
}

/// @brief SCP reset template id 120 clears dictionary state without a body.
TEST(QuickFAST, testDecoderAcceptsScpResetTemplate)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\">"
    "<uInt32 name=\"value\"><copy/></uInt32></template></templates>");

  // pmap with tid bit, tid=120 (0x80 | 120 = 0xF8)
  const std::string resetWire("\xC0\xF8", 2);
  Codecs::DataSourceString source(resetWire);
  Codecs::Decoder decoder(registry);
  RecordingBuilder builder;
  EXPECT_NO_THROW(decoder.decodeMessage(source, builder));
  EXPECT_EQ(0u, builder.messages_);
}

/// @brief ignore=\"true\" templates decode then discard via ignoreMessage.
TEST(QuickFAST, testDecoderIgnoresMarkedTemplates)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\" ignore=\"true\">"
    "<uInt32 name=\"value\"/></template></templates>");
  const std::string fast = encodeUInt32(registry, 1, 5u);

  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  RecordingBuilder builder;
  decoder.decodeMessage(source, builder);
  EXPECT_EQ(0u, builder.messages_);
  EXPECT_EQ(1u, builder.ignored_);
}

/// @brief Context logging and non-strict [ERR D2] must honour their contracts.
TEST(QuickFAST, testContextLoggingAndNonStrictD2)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\">"
    "<uInt32 name=\"value\"/></template></templates>");
  Codecs::Decoder decoder(registry);

  std::ostringstream log;
  std::ostringstream verbose;
  decoder.setLogOutput(log);
  decoder.setVerboseOutput(verbose);

  decoder.logMessage("hello-log");
  EXPECT_NE(std::string::npos, log.str().find("hello-log"));

  decoder.reportWarning("[ERR W1]", "warn");
  decoder.reportWarning("[ERR W1]", "warn-field", valueIdentity);
  decoder.reportWarning("[ERR W1]", "warn-name", "value");
  EXPECT_NE(std::string::npos, log.str().find("[ERR W1]"));

  Codecs::TemplateCPtr found;
  EXPECT_TRUE(decoder.findTemplate("t", "", found));

  decoder.setStrict(false);
  EXPECT_NO_THROW(decoder.reportError("[ERR D2]", "overflow"));
  EXPECT_THROW(decoder.reportError("[ERR D9]", "other"), EncodingError);
  EXPECT_THROW(decoder.reportFatal("[ERR F1]", "fatal"), EncodingError);
  EXPECT_THROW(
    decoder.reportFatal("[ERR F1]", "fatal-field", valueIdentity),
    EncodingError);
}

/// @brief FieldOp::opName must name every operator, including UNKNOWN.
TEST(QuickFAST, testFieldOpNames)
{
  EXPECT_EQ("nop", Codecs::FieldOp::opName(Codecs::FieldOp::NOP));
  EXPECT_EQ("constant", Codecs::FieldOp::opName(Codecs::FieldOp::CONSTANT));
  EXPECT_EQ("default", Codecs::FieldOp::opName(Codecs::FieldOp::DEFAULT));
  EXPECT_EQ("copy", Codecs::FieldOp::opName(Codecs::FieldOp::COPY));
  EXPECT_EQ("delta", Codecs::FieldOp::opName(Codecs::FieldOp::DELTA));
  EXPECT_EQ("increment", Codecs::FieldOp::opName(Codecs::FieldOp::INCREMENT));
  EXPECT_EQ("tail", Codecs::FieldOp::opName(Codecs::FieldOp::TAIL));
  EXPECT_EQ("UNKNOWN", Codecs::FieldOp::opName(Codecs::FieldOp::UNKNOWN));
}

/// @brief PresenceMap verbose encode/decode must still round-trip the bits.
TEST(QuickFAST, testPresenceMapVerboseEncodeDecode)
{
  Codecs::PresenceMap pmap(10);
  for(size_t bit = 0; bit < 10; ++bit)
  {
    pmap.setNextField(bit % 3 != 0);
  }

  std::ostringstream verbose;
  pmap.setVerbose(&verbose);

  Codecs::DataDestination destination;
  pmap.encode(destination);
  destination.endMessage();
  std::string wire;
  destination.toString(wire);
  ASSERT_FALSE(wire.empty());
  EXPECT_FALSE(verbose.str().empty());

  Codecs::DataSourceString source(wire);
  Codecs::PresenceMap decoded(1);
  decoded.setVerbose(&verbose);
  decoded.decode(source);
  decoded.rewind();
  pmap.rewind();
  for(size_t bit = 0; bit < 10; ++bit)
  {
    EXPECT_EQ(pmap.checkNextField(), decoded.checkNextField()) << bit;
  }
}

/// @brief Template::display must emit namespaces, reset and ignore markers.
TEST(QuickFAST, testTemplateDisplayIncludesAttributes)
{
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates>"
    "  <template name=\"shown\" id=\"9\" templateNs=\"TNS\" ns=\"NS\""
    "            dictionary=\"session\" reset=\"true\" ignore=\"true\">"
    "    <uInt32 name=\"value\"/>"
    "  </template>"
    "</templates>");

  std::ostringstream out;
  registry->display(out);
  const std::string text = out.str();
  EXPECT_NE(std::string::npos, text.find("id=\"9\""));
  EXPECT_NE(std::string::npos, text.find("name=\"shown\""));
  EXPECT_NE(std::string::npos, text.find("templateNs=\"TNS\""));
  EXPECT_NE(std::string::npos, text.find("ns=\"NS\""));
  EXPECT_NE(std::string::npos, text.find("dictionary=\"session\""));
  EXPECT_NE(std::string::npos, text.find("reset=\"Y\""));
  EXPECT_NE(std::string::npos, text.find("NO OUTPUT WILL BE GENERATED"));
}

/// @brief SingleMessageConsumer logging hooks must be callable without crash.
TEST(QuickFAST, testSingleMessageConsumerLoggingHooks)
{
  Codecs::SingleMessageConsumer consumer;
  EXPECT_TRUE(consumer.wantLog(1));
  EXPECT_TRUE(consumer.logMessage(1, "log-line"));
  EXPECT_TRUE(consumer.reportDecodingError("decode-err"));
  EXPECT_TRUE(consumer.reportCommunicationError("comm-err"));
  consumer.decodingStarted();
  consumer.decodingStopped();
  (void)consumer.message();
}

/// @brief A packet header that asks to skip must not decode the body.
TEST(QuickFAST, testBasePacketAssemblerHonoursSkipBlock)
{
  SkippingPacketHeader packetHeader;
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::TemplateRegistryPtr registry = parseTemplates(
    "<templates><template name=\"t\" id=\"1\">"
    "<uInt32 name=\"value\"/></template></templates>");
  TestAssembler assembler(registry, packetHeader, messageHeader, builder);

  const std::string packet("\xC0\x81\x82", 3);
  EXPECT_TRUE(assembler.decodeBuffer(
    reinterpret_cast<const unsigned char *>(packet.data()), packet.size()));
  EXPECT_EQ(0u, builder.messages_);
  EXPECT_EQ(packet.size(), assembler.byteCount());
  (void)assembler.decoder();
}
