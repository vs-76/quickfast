// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// The same FAST bytes must decode identically through every DataSource that
// applications actually use. DataSourceBufferedStream had zero coverage and is
// the path performance tests take; if it disagreed with DataSourceString the
// bench would measure a different codec than production.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/DataSourceBuffer.h>
#include <Codecs/DataSourceStream.h>
#include <Codecs/DataSourceBufferedStream.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldUInt32.h>

#include <cstring>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr registry()
  {
    std::stringstream xml(
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <uInt32 name=\"value\"><nop/></uInt32>"
      "  </template>"
      "</templates>");
    Codecs::XMLTemplateParser parser;
    return parser.parse(xml);
  }

  std::string encode(uint32 value)
  {
    Codecs::TemplateRegistryPtr templates = registry();
    Messages::Message message(templates->maxFieldCount());
    message.addField(valueIdentity, Messages::FieldUInt32::create(value));

    Codecs::Encoder encoder(templates);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, 1, message);
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  uint32 decodeFrom(Codecs::DataSource & source)
  {
    Codecs::Decoder decoder(registry());
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    decoder.decodeMessage(source, builder);

    Messages::FieldCPtr field;
    EXPECT_TRUE(consumer.message().getField("value", field));
    return field->toUInt32();
  }
}

/// @brief String, buffer, stream and buffered-stream sources must agree.
TEST(QuickFAST, testDataSourceVariantsDecodeTheSameMessage)
{
  const uint32 expected = 42u;
  const std::string fast = encode(expected);

  {
    Codecs::DataSourceString source(fast);
    EXPECT_EQ(expected, decodeFrom(source));
  }
  {
    Codecs::DataSourceBuffer source(
      reinterpret_cast<const unsigned char *>(fast.data()), fast.size());
    EXPECT_EQ(expected, decodeFrom(source));
  }
  {
    std::stringstream stream(fast);
    Codecs::DataSourceStream source(stream);
    EXPECT_EQ(expected, decodeFrom(source));
  }
  {
    std::stringstream stream(fast);
    Codecs::DataSourceBufferedStream source(stream);
    EXPECT_EQ(expected, decodeFrom(source));
  }
}

/// @brief A second getBuffer from DataSourceBufferedStream must report EOF.
TEST(QuickFAST, testBufferedStreamReportsEndAfterOneBuffer)
{
  const std::string fast = encode(7u);
  std::stringstream stream(fast);
  Codecs::DataSourceBufferedStream source(stream);

  const uchar * buffer = 0;
  size_t size = 0;
  ASSERT_TRUE(source.getBuffer(buffer, size));
  EXPECT_EQ(fast.size(), size);
  EXPECT_EQ(0, std::memcmp(buffer, fast.data(), size));

  EXPECT_FALSE(source.getBuffer(buffer, size));
}
