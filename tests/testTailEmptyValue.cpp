// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// An empty tail arises two ways, and they are not the same thing.
//
// The tail operator sets the presence map bit false to say "no tail to send,
// the value is unchanged". The encoder computed the tail as the part of the
// value beyond its common prefix with the previous value, and set the bit
// false whenever that came out empty -- which is also what happens when the
// value itself is empty and there is no previous value to be unchanged from.
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
#include <Messages/FieldByteVector.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr tailRegistry(
    const std::string & fastType, bool optional)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <" + fastType + " name=\"value\"" +
             std::string(optional ? " presence=\"optional\"" : "") + ">"
      "      <tail/>"
      "    </" + fastType + ">"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  Messages::FieldCPtr makeField(
    const std::string & fastType, const std::string & value)
  {
    if(fastType == "string")
    {
      return Messages::FieldAscii::create(value);
    }
    return Messages::FieldByteVector::create(value);
  }

  /// @brief Send one message through a fresh encoder and decoder.
  /// @return false if the decoder reported the field absent.
  bool roundTrip(
    const std::string & fastType,
    bool optional,
    const std::string & value,
    std::string & decoded)
  {
    Codecs::TemplateRegistryPtr registry = tailRegistry(fastType, optional);

    Messages::Message message(registry->maxFieldCount());
    message.addField(valueIdentity, makeField(fastType, value));

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
    if(!consumer.message().getField("value", field))
    {
      return false;
    }
    decoded = static_cast<std::string>(
      fastType == "string" ? field->toAscii() : field->toByteVector());
    return true;
  }
}

/// @brief An empty value through a mandatory tail field must round-trip.
///
/// It used to produce a stream the library's own decoder rejected, with
/// [ERR D6] "No value available for mandatory copy field" -- the encoder said
/// "unchanged" and the decoder found nothing to be unchanged from.
TEST(QuickFAST, testMandatoryTailEncodesAnEmptyValue)
{
  for(const char * fastType : {"string", "byteVector"})
  {
    SCOPED_TRACE(fastType);
    std::string decoded = "unset";
    ASSERT_TRUE(roundTrip(fastType, false, "", decoded));
    EXPECT_EQ("", decoded);
  }
}

/// @brief An optional tail field must deliver an empty value, not absence.
///
/// Present-and-empty and absent are different facts, and the encoder collapsed
/// the first into the second.
TEST(QuickFAST, testOptionalTailEncodesAnEmptyValue)
{
  for(const char * fastType : {"string", "byteVector"})
  {
    SCOPED_TRACE(fastType);
    std::string decoded = "unset";
    ASSERT_TRUE(roundTrip(fastType, true, "", decoded))
      << "the decoder saw the field as absent";
    EXPECT_EQ("", decoded);
  }
}

/// @brief Non-empty values through tail must be unaffected.
TEST(QuickFAST, testTailStillCarriesNonEmptyValues)
{
  for(const char * fastType : {"string", "byteVector"})
  {
    for(const char * value : {"A", "hello", "a longer value"})
    {
      SCOPED_TRACE(std::string(fastType) + " \"" + value + "\"");
      std::string decoded;
      ASSERT_TRUE(roundTrip(fastType, false, value, decoded));
      EXPECT_EQ(std::string(value), decoded);

      std::string optionalDecoded;
      ASSERT_TRUE(roundTrip(fastType, true, value, optionalDecoded));
      EXPECT_EQ(std::string(value), optionalDecoded);
    }
  }
}

/// @brief A tail field that cannot resolve its value must say so accurately.
///
/// Tail decoding reuses the copy path, and the message went along with it, so
/// the one diagnostic a user gets here named the wrong operator.
TEST(QuickFAST, testTailReportsTailNotCopy)
{
  // A mandatory tail field with the pmap bit clear and no dictionary value.
  Codecs::TemplateRegistryPtr registry = tailRegistry("string", false);
  const std::string fast("\xc0\x81", 2); // pmap: template id present, tail absent

  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);

  try
  {
    decoder.decodeMessage(source, builder);
    FAIL() << "expected the decoder to reject the message";
  }
  catch(const std::exception & ex)
  {
    const std::string what(ex.what());
    EXPECT_NE(std::string::npos, what.find("tail"))
      << "diagnostic does not mention the tail operator: " << what;
    EXPECT_EQ(std::string::npos, what.find("copy"))
      << "diagnostic blames the copy operator: " << what;
  }
}
