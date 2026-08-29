// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A FAST ascii string is seven-bit, and the encoder relies on that without
// checking it. The stop bit is the eighth, so a byte that already has it set
// terminates the string early or has its value quietly changed. Putting UTF-8
// text into a <string> field is one of the most common integration mistakes
// there is, and the library's answer was to shorten the message and mangle a
// byte without a word.
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

#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr asciiRegistry(
    const std::string & fieldOperator, bool optional)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <string name=\"value\"" +
             std::string(optional ? " presence=\"optional\"" : "") + ">" +
             fieldOperator +
      "    </string>"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief Encode one string field. Decoding is deliberately separate: for
  /// these inputs the encode step is where the answer is decided.
  std::string encode(
    const Codecs::TemplateRegistryPtr & registry, const std::string & value)
  {
    Messages::Message message(registry->maxFieldCount());
    message.addField(valueIdentity, Messages::FieldAscii::create(value));

    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, 1, message);
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  /// @brief Encode then decode, reporting whether the field survived.
  bool roundTrip(
    const Codecs::TemplateRegistryPtr & registry,
    const std::string & value,
    std::string & decoded)
  {
    const std::string fast = encode(registry, value);

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
    decoded = static_cast<std::string>(field->toAscii());
    return true;
  }

  const char * const theOperators[] =
  {
    "<nop/>", "<copy/>", "<default/>", "<delta/>", "<tail/>"
  };

  /// @brief Inputs a caller might plausibly hand to a string field.
  ///
  /// "hi\xc3\xa9" is the UTF-8 for "hie" with an acute accent -- accented text
  /// in a name or symbol field, which used to arrive as "hiC".
  const char * const theEightBitValues[] =
  {
    "\x80", "\xff", "A\x80", "\x80" "A", "hi\xc3\xa9"
  };
}

/// @brief A byte with the high bit set must be refused, not quietly mangled.
TEST(QuickFAST, testAsciiRejectsHighBitBytes)
{
  for(const char * fieldOperator : theOperators)
  {
    for(bool optional : {false, true})
    {
      for(const char * value : theEightBitValues)
      {
        SCOPED_TRACE(std::string(fieldOperator) +
          (optional ? " optional" : " mandatory"));
        Codecs::TemplateRegistryPtr registry =
          asciiRegistry(fieldOperator, optional);
        EXPECT_THROW((void)encode(registry, value), EncodingError);
      }
    }
  }
}

/// @brief Seven-bit values must be untouched by the new check.
TEST(QuickFAST, testAsciiRoundTripsSevenBitValues)
{
  const char * const values[] =
  {
    "", "A", "hello", "a longer value with spaces", "\x7f", "0123456789"
  };

  for(const char * fieldOperator : theOperators)
  {
    for(const char * value : values)
    {
      SCOPED_TRACE(std::string(fieldOperator) + " \"" + value + "\"");
      Codecs::TemplateRegistryPtr registry = asciiRegistry(fieldOperator, false);
      std::string decoded;
      // The empty value through tail is finding #77, fixed separately.
      if(std::string(value).empty() && std::string(fieldOperator) == "<tail/>")
      {
        continue;
      }
      ASSERT_TRUE(roundTrip(registry, value, decoded));
      EXPECT_EQ(std::string(value), decoded);
    }
  }
}

/// @brief The embedded NUL convention that the encoder does support still works.
///
/// A leading zero byte gets a preamble rather than being written verbatim, so
/// it must not be caught by the high-bit check.
TEST(QuickFAST, testAsciiRoundTripsLeadingZeroByte)
{
  Codecs::TemplateRegistryPtr registry = asciiRegistry("<nop/>", false);
  std::string decoded;
  ASSERT_TRUE(roundTrip(registry, std::string("\0", 1), decoded));
  EXPECT_EQ(std::string("\0", 1), decoded);
}
