// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Two questions about string lengths on the decode side.
//
// A tail value longer than the base value: the FAST 1.1 specification answers
// this directly in 6.3.8.1 and 6.3.8.3 -- "if the length of the tail value
// exceeds the length of the base value, the combined value becomes the tail
// value" -- and the clamp in decodeTail produces exactly that. These tests pin
// it down so it is not mistaken for a truncation bug and "fixed" into an error.
//
// A delta length longer than the base value is the opposite case: the spec
// defines [ERR D7] for it and the decoder throws, which leaves the repair
// assignment after each reportError call unreachable.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Messages/Message.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  Codecs::TemplateRegistryPtr registryFor(
    const std::string & fastType,
    const std::string & fieldOperator)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <" + fastType + " name=\"value\">" + fieldOperator +
      "    </" + fastType + ">"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief Decode a run of messages, returning each value as a string.
  std::vector<std::string> decodeRun(
    const Codecs::TemplateRegistryPtr & registry,
    const std::string & fast,
    size_t messageCount)
  {
    Codecs::DataSourceString source(fast);
    Codecs::Decoder decoder(registry);
    std::vector<std::string> values;
    for(size_t message = 0; message < messageCount; ++message)
    {
      Codecs::SingleMessageConsumer consumer;
      Codecs::GenericMessageBuilder builder(consumer);
      decoder.decodeMessage(source, builder);
      Messages::FieldCPtr field;
      if(consumer.message().getField("value", field))
      {
        values.push_back(field->toString());
      }
    }
    return values;
  }

  /// @brief An ASCII string in the FAST transfer encoding.
  std::string ascii(const std::string & text)
  {
    std::string encoded(text);
    encoded.back() |= '\x80';
    return encoded;
  }
}

/// @brief A tail longer than the base value replaces it, per 6.3.8.1.
TEST(QuickFAST, testAsciiTailLongerThanBaseBecomesTheValue)
{
  Codecs::TemplateRegistryPtr registry = registryFor("string", "<tail/>");

  // Message one: pmap says the tail is present, and with no previous value the
  // base is the empty string, so "AB" arrives whole.
  // Message two: a tail of "WXYZ" against a base of "AB". The tail is longer,
  // so the combined value is the tail.
  const std::string fast =
    std::string("\xE0\x81", 2) + ascii("AB") +
    std::string("\xE0\x81", 2) + ascii("WXYZ");

  const std::vector<std::string> values = decodeRun(registry, fast, 2);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("AB", values[0]);
  EXPECT_EQ("WXYZ", values[1]);
}

/// @brief A tail shorter than the base value overlays its back, as intended.
TEST(QuickFAST, testAsciiTailShorterThanBaseOverlaysTheBack)
{
  Codecs::TemplateRegistryPtr registry = registryFor("string", "<tail/>");

  const std::string fast =
    std::string("\xE0\x81", 2) + ascii("ABCDEFGH") +
    std::string("\xE0\x81", 2) + ascii("XY");

  const std::vector<std::string> values = decodeRun(registry, fast, 2);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("ABCDEFGH", values[0]);
  EXPECT_EQ("ABCDEFXY", values[1]);
}

/// @brief A byteVector tail longer than the base replaces it, per 6.3.8.3.
TEST(QuickFAST, testByteVectorTailLongerThanBaseBecomesTheValue)
{
  Codecs::TemplateRegistryPtr registry = registryFor("byteVector", "<tail/>");

  const std::string fast =
    std::string("\xE0\x81\x82" "AB", 5) +
    std::string("\xE0\x81\x84" "WXYZ", 7);

  const std::vector<std::string> values = decodeRun(registry, fast, 2);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("AB", values[0]);
  EXPECT_EQ("WXYZ", values[1]);
}

/// @brief A back delta longer than the base value is [ERR D7].
TEST(QuickFAST, testAsciiBackDeltaLongerThanBaseIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("string", "<delta/>");

  // A subtraction of five against a base of "AB".
  const std::string fast =
    std::string("\xC0\x81\x80", 3) + ascii("AB") +
    std::string("\xC0\x81\x85", 3) + ascii("Z");

  EXPECT_THROW((void)decodeRun(registry, fast, 2), EncodingError);
}

/// @brief A front delta longer than the base value is [ERR D7] too.
TEST(QuickFAST, testAsciiFrontDeltaLongerThanBaseIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("string", "<delta/>");

  // Subtraction lengths are sent excess -1 on the front, so 0xFA is -6.
  const std::string fast =
    std::string("\xC0\x81\x80", 3) + ascii("AB") +
    std::string("\xC0\x81\xFA", 3) + ascii("Z");

  EXPECT_THROW((void)decodeRun(registry, fast, 2), EncodingError);
}

/// @brief A byteVector back delta longer than the base is [ERR D7].
TEST(QuickFAST, testByteVectorDeltaLongerThanBaseIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("byteVector", "<delta/>");

  const std::string fast =
    std::string("\xC0\x81\x80\x82" "AB", 6) +
    std::string("\xC0\x81\x85\x81" "Z", 5);

  EXPECT_THROW((void)decodeRun(registry, fast, 2), EncodingError);
}

/// @brief Deltas within the base length still apply.
TEST(QuickFAST, testAsciiDeltaWithinBaseLengthStillApplies)
{
  Codecs::TemplateRegistryPtr registry = registryFor("string", "<delta/>");

  const std::string fast =
    std::string("\xC0\x81\x80", 3) + ascii("ABCDEF") +
    std::string("\xC0\x81\x82", 3) + ascii("XY");

  const std::vector<std::string> values = decodeRun(registry, fast, 2);
  ASSERT_EQ(2u, values.size());
  EXPECT_EQ("ABCDEF", values[0]);
  EXPECT_EQ("ABCDXY", values[1]);
}
