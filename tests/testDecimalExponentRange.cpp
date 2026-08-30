// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// The decimal exponent travels as a signed integer field, so it inherited the
// nullable-adjustment overflow that #61 fixed for integer fields. exponent_t
// is an int8, and for an optional decimal the encoder adds one to the exponent
// to make room for the null sentinel. At 127 that wraps to -128, which
// multiplies the decoded value by 10^-255 with the mantissa intact and nothing
// reported.
//
// FAST 1.1 confines a decimal exponent to [-63, 63], so 127 is out-of-spec
// input a conforming counterparty would never send. It is still worth fixing:
// FieldDecimal::create takes it without complaint, and both directions of the
// adjustment need the wider intermediate for the same reason the integer
// fields did.
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
#include <Messages/FieldDecimal.h>
#include <Messages/FieldIdentity.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr registryFor(
    const std::string & fieldOperator,
    bool optional)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <decimal name=\"value\"" +
             std::string(optional ? " presence=\"optional\"" : "") + ">" +
             fieldOperator +
      "    </decimal>"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief Encode one decimal and decode it back.
  Decimal roundTrip(
    const std::string & fieldOperator,
    bool optional,
    mantissa_t mantissa,
    exponent_t exponent)
  {
    Codecs::TemplateRegistryPtr registry = registryFor(fieldOperator, optional);

    Messages::Message message(registry->maxFieldCount());
    message.addField(
      valueIdentity, Messages::FieldDecimal::create(Decimal(mantissa, exponent, false)));

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
    EXPECT_TRUE(consumer.message().getField("value", field));
    return field->toDecimal();
  }
}

/// @brief An out-of-range exponent must be refused rather than corrupted.
///
/// delta has always refused it, since it range checks the result of applying
/// the delta. nop, copy and default encoded it, and for an optional field the
/// nullable adjustment then wrapped 127 to -128. All four now agree.
TEST(QuickFAST, testOutOfRangeDecimalExponentIsReported)
{
  const char * operators[] = {"<nop/>", "<copy/>", "<default/>", "<delta/>"};
  const exponent_t exponents[] = {127, 126, 64, -64, -128};
  for(const char * fieldOperator : operators)
  {
    for(bool optional : {false, true})
    {
      for(exponent_t exponent : exponents)
      {
        EXPECT_THROW(
          (void)roundTrip(fieldOperator, optional, 1, exponent),
          EncodingError)
          << fieldOperator << " optional=" << optional
          << " exponent=" << int(exponent);
      }
    }
  }
}

/// @brief Every exponent the specification allows still round-trips.
TEST(QuickFAST, testLegalDecimalExponentsRoundTrip)
{
  const char * operators[] = {"<nop/>", "<copy/>", "<default/>", "<delta/>"};
  const mantissa_t mantissas[] = {1, -1, 12345};
  for(const char * fieldOperator : operators)
  {
    for(bool optional : {false, true})
    {
      for(int exponent = -63; exponent <= 63; ++exponent)
      {
        for(mantissa_t mantissa : mantissas)
        {
          const Decimal result =
            roundTrip(fieldOperator, optional, mantissa, exponent_t(exponent));
          EXPECT_EQ(mantissa, result.getMantissa())
            << fieldOperator << " optional=" << optional
            << " exponent=" << exponent;
          EXPECT_EQ(exponent, int(result.getExponent()))
            << fieldOperator << " optional=" << optional
            << " exponent=" << exponent;
        }
      }
    }
  }
}
