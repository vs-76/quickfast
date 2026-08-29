// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A value oracle for the integer field operators at the extremes of each type.
//
// The existing operator coverage (testRoundTripFieldOps) uses values 1-7, and
// testBiggestValue exercises the decode direction only. Neither can see an
// encoder that corrupts a value on the way out, because a corrupted value is
// still well-formed FAST: the bytes decode cleanly, they just mean something
// else. Only encoding a known value and reading it back catches that.
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
#include <Messages/FieldInt32.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldInt64.h>
#include <Messages/FieldUInt64.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  /// @brief A one-field template registry, built for a single (type, operator) pair.
  Codecs::TemplateRegistryPtr singleFieldRegistry(
    const std::string & fastType,
    const std::string & fieldOperator,
    bool optional)
  {
    std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <" + fastType + " name=\"value\"" +
             (optional ? " presence=\"optional\"" : "") + ">" +
             fieldOperator +
      "    </" + fastType + ">"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief Encode one field, decode it back, and report what survived.
  ///
  /// A fresh encoder and decoder per call: sharing them would carry dictionary
  /// state between cases, and a copy or increment operator can then transmit
  /// nothing at all, which would hide a broken value behind a correct one.
  ///
  /// @param field the value to encode.
  /// @param[out] decoded the field the decoder produced.
  /// @return true if the decoder saw the field at all; false means it decoded
  ///         as absent, which for an optional field is a legal but wrong answer.
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

  /// @brief Round-trip an unsigned value and require it back unchanged.
  void expectUnsignedSurvives(
    const std::string & fastType,
    const std::string & fieldOperator,
    bool optional,
    uint64 value)
  {
    SCOPED_TRACE(fastType + " " + fieldOperator +
      (optional ? " optional" : " mandatory") +
      " = " + std::to_string(value));

    Codecs::TemplateRegistryPtr registry =
      singleFieldRegistry(fastType, fieldOperator, optional);
    Messages::FieldCPtr decoded;
    ASSERT_TRUE(roundTrip(registry, Messages::FieldUInt64::create(value), decoded))
      << "the decoder saw the field as absent";
    EXPECT_EQ(value, decoded->toUnsignedInteger());
  }

  /// @brief Round-trip a signed value and require it back unchanged.
  void expectSignedSurvives(
    const std::string & fastType,
    const std::string & fieldOperator,
    bool optional,
    int64 value)
  {
    SCOPED_TRACE(fastType + " " + fieldOperator +
      (optional ? " optional" : " mandatory") +
      " = " + std::to_string(value));

    Codecs::TemplateRegistryPtr registry =
      singleFieldRegistry(fastType, fieldOperator, optional);
    Messages::FieldCPtr decoded;
    ASSERT_TRUE(roundTrip(registry, Messages::FieldInt64::create(value), decoded))
      << "the decoder saw the field as absent";
    EXPECT_EQ(value, decoded->toSignedInteger());
  }

  /// @brief The operators that put a value on the wire through the increment path.
  ///
  /// delta is deliberately absent: it does its nullable adjustment in int64
  /// already, and its own overflow at the 64 bit extremes is a separate finding.
  const char * const theOperators[] =
  {
    "<nop/>",
    "<default/>",
    "<copy/>",
    "<increment/>"
  };
}

/// @brief The maximum of a 32 bit optional field must survive the encoder.
///
/// The adjustment that reserves zero for null was applied in the field's own
/// type, so the maximum wrapped: unsigned to zero, which *is* the null
/// encoding, and signed to the type minimum. Both produce valid FAST carrying
/// the wrong fact, which is why no sanitizer and no fuzzer reported it.
TEST(QuickFAST, testOptionalInteger32MaximumRoundTrips)
{
  for(const char * fieldOperator : theOperators)
  {
    expectUnsignedSurvives("uInt32", fieldOperator, true,
      std::numeric_limits<uint32>::max());
    expectSignedSurvives("int32", fieldOperator, true,
      std::numeric_limits<int32>::max());
  }
}

/// @brief Values just below the maximum were always fine, and must stay so.
TEST(QuickFAST, testOptionalIntegerNearMaximumRoundTrips)
{
  for(const char * fieldOperator : theOperators)
  {
    expectUnsignedSurvives("uInt32", fieldOperator, true,
      std::numeric_limits<uint32>::max() - 1);
    expectSignedSurvives("int32", fieldOperator, true,
      std::numeric_limits<int32>::max() - 1);
    expectSignedSurvives("int32", fieldOperator, true,
      std::numeric_limits<int32>::min());
  }
}

/// @brief A mandatory field has no adjustment, so its maximum is representable.
TEST(QuickFAST, testMandatoryIntegerMaximumRoundTrips)
{
  for(const char * fieldOperator : theOperators)
  {
    expectUnsignedSurvives("uInt32", fieldOperator, false,
      std::numeric_limits<uint32>::max());
    expectUnsignedSurvives("uInt64", fieldOperator, false,
      std::numeric_limits<uint64>::max());
    expectSignedSurvives("int32", fieldOperator, false,
      std::numeric_limits<int32>::max());
    expectSignedSurvives("int64", fieldOperator, false,
      std::numeric_limits<int64>::max());
  }
}

/// @brief At 64 bits the adjusted value is unrepresentable, so it must be refused.
///
/// value + 1 has nowhere to go: widening cannot help and the decoder could not
/// hold the result either. FAST simply cannot carry the maximum of an optional
/// 64 bit field. Reporting that beats transmitting a plausible wrong number.
TEST(QuickFAST, testOptionalInteger64MaximumIsRejected)
{
  for(const char * fieldOperator : theOperators)
  {
    SCOPED_TRACE(fieldOperator);
    {
      Codecs::TemplateRegistryPtr registry =
        singleFieldRegistry("uInt64", fieldOperator, true);
      Messages::FieldCPtr decoded;
      EXPECT_THROW(
        roundTrip(registry,
          Messages::FieldUInt64::create(std::numeric_limits<uint64>::max()),
          decoded),
        EncodingError);
    }
    {
      Codecs::TemplateRegistryPtr registry =
        singleFieldRegistry("int64", fieldOperator, true);
      Messages::FieldCPtr decoded;
      EXPECT_THROW(
        roundTrip(registry,
          Messages::FieldInt64::create(std::numeric_limits<int64>::max()),
          decoded),
        EncodingError);
    }
  }
}
