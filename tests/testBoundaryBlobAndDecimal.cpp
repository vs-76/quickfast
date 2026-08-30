// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Part 16-style boundary matrix for decimal, ASCII/UTF-8 strings and blobs.
// Integer extremes live in testBoundaryValueRoundTrip; ASCII high-bit rejection
// lives in testAsciiEncoding. This file covers the remaining value oracles:
// mantissa extremes, embedded NULs, binary-safe byteVector, and utf8.
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

#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldUtf8.h>
#include <Messages/FieldByteVector.h>
#include <Messages/FieldDecimal.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr singleFieldRegistry(
    const std::string & element,
    const std::string & fieldOperator,
    bool optional)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <" + element + " name=\"value\"" +
             (optional ? " presence=\"optional\"" : "") + ">" +
             fieldOperator +
      "    </" + element.substr(0, element.find(' ')) + ">"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  bool roundTrip(
    const Codecs::TemplateRegistryPtr & registry,
    Messages::FieldCPtr field,
    Messages::FieldCPtr & decoded)
  {
    Messages::Message message(registry->maxFieldCount());
    message.addField(valueIdentity, field);

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

    return consumer.message().getField("value", decoded);
  }

  const char * const blobOperators[] =
  {
    "<nop/>", "<copy/>", "<default/>", "<delta/>", "<tail/>"
  };

  const char * const decimalOperators[] =
  {
    "<nop/>", "<copy/>", "<default/>", "<delta/>"
  };

  /// @brief Values that stress empty / NUL / high-bit wire rules.
  const std::string blobValues[] =
  {
    std::string(""),
    std::string("\0", 1),
    std::string("\0\0", 2),
    std::string("\0A", 2),
    std::string("A\0", 2),
    std::string("A\0B", 3),
    std::string("\x80", 1),
    std::string("\xff", 1),
    std::string("hi\xc3\xa9", 4)
  };
}

/// @brief Spec-legal decimal mantissas at the type extremes must survive.
TEST(QuickFAST, testDecimalMantissaExtremesRoundTrip)
{
  const mantissa_t mantissas[] =
  {
    0,
    1,
    -1,
    std::numeric_limits<mantissa_t>::max(),
    std::numeric_limits<mantissa_t>::min()
  };
  const exponent_t exponents[] = {0, 1, -1, 63, -63};

  for(const char * fieldOperator : decimalOperators)
  {
    for(bool optional : {false, true})
    {
      for(mantissa_t mantissa : mantissas)
      {
        for(exponent_t exponent : exponents)
        {
          SCOPED_TRACE(
            std::string(fieldOperator) +
            (optional ? " optional" : " mandatory") +
            " m=" + std::to_string(mantissa) +
            " e=" + std::to_string(int(exponent)));

          Codecs::TemplateRegistryPtr registry =
            singleFieldRegistry("decimal", fieldOperator, optional);
          Messages::FieldCPtr decoded;
          ASSERT_TRUE(roundTrip(
            registry,
            Messages::FieldDecimal::create(Decimal(mantissa, exponent, false)),
            decoded));
          const Decimal value = decoded->toDecimal();
          EXPECT_EQ(mantissa, value.getMantissa());
          EXPECT_EQ(exponent, value.getExponent());
        }
      }
    }
  }
}

/// @brief byteVector is binary-safe: high bits and NULs round-trip under every op.
TEST(QuickFAST, testByteVectorBoundaryValuesRoundTrip)
{
  for(const char * fieldOperator : blobOperators)
  {
    for(bool optional : {false, true})
    {
      for(const std::string & value : blobValues)
      {
        // Empty through mandatory tail is #77; covered in testTailEmptyValue.
        if(value.empty() && std::string(fieldOperator) == "<tail/>")
        {
          continue;
        }
        SCOPED_TRACE(
          std::string(fieldOperator) +
          (optional ? " optional" : " mandatory") +
          " len=" + std::to_string(value.size()));

        Codecs::TemplateRegistryPtr registry =
          singleFieldRegistry("byteVector", fieldOperator, optional);
        Messages::FieldCPtr decoded;
        ASSERT_TRUE(roundTrip(
          registry, Messages::FieldByteVector::create(value), decoded));
        EXPECT_EQ(value, static_cast<std::string>(decoded->toByteVector()));
      }
    }
  }
}

/// @brief Seven-bit ASCII with embedded NULs still uses the preamble path correctly.
TEST(QuickFAST, testAsciiEmbeddedNulRoundTrips)
{
  const std::string values[] =
  {
    std::string("\0", 1),
    std::string("\0\0", 2),
    std::string("\0A", 2),
    std::string("A\0", 2),
    std::string("A\0B", 3)
  };

  for(const char * fieldOperator : blobOperators)
  {
    for(const std::string & value : values)
    {
      SCOPED_TRACE(std::string(fieldOperator) + " len=" + std::to_string(value.size()));
      Codecs::TemplateRegistryPtr registry =
        singleFieldRegistry("string", fieldOperator, false);
      Messages::FieldCPtr decoded;
      ASSERT_TRUE(roundTrip(
        registry, Messages::FieldAscii::create(value), decoded));
      EXPECT_EQ(value, static_cast<std::string>(decoded->toAscii()));
    }
  }
}

/// @brief UTF-8 strings must preserve multi-byte characters the ASCII path rejects.
TEST(QuickFAST, testUtf8BoundaryValuesRoundTrip)
{
  const std::string values[] =
  {
    std::string(""),
    std::string("A"),
    std::string("hi\xc3\xa9", 4),
    std::string("\0", 1),
    std::string("\xff\xfe", 2)
  };

  for(const char * fieldOperator : blobOperators)
  {
    for(bool optional : {false, true})
    {
      for(const std::string & value : values)
      {
        if(value.empty() && std::string(fieldOperator) == "<tail/>")
        {
          continue;
        }
        SCOPED_TRACE(
          std::string(fieldOperator) +
          (optional ? " optional" : " mandatory") +
          " len=" + std::to_string(value.size()));

        Codecs::TemplateRegistryPtr registry = singleFieldRegistry(
          "string charset=\"unicode\"", fieldOperator, optional);
        Messages::FieldCPtr decoded;
        ASSERT_TRUE(roundTrip(
          registry, Messages::FieldUtf8::create(value), decoded));
        EXPECT_EQ(value, static_cast<std::string>(decoded->toUtf8()));
      }
    }
  }
}
