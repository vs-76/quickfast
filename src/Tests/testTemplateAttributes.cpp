// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Three attributes that the template parser reads carelessly.
//
// A boolean attribute's first byte is handed to toupper as a plain char, which
// is signed here, so any value starting above 0x7F -- and template files are
// UTF-8 -- passes a negative int to a function that requires an unsigned char.
//
// pmap is parsed with atoi, which cannot report failure, so pmap="abc" means
// bit zero and pmap="-1" means SIZE_MAX.
//
// ignore_overflows reaches exactly one of the eight integer parsers. On the
// other seven the attribute is read from the file and thrown away, and it
// bypasses the validation every other boolean attribute gets.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Template.h>
#include <Codecs/FieldInstruction.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  Codecs::TemplateRegistryPtr parse(const std::string & body)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">" + body +
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief The single field instruction of the single template.
  Codecs::FieldInstructionCPtr onlyField(const Codecs::TemplateRegistryPtr & registry)
  {
    Codecs::TemplateCPtr templatePtr;
    EXPECT_TRUE(registry->getTemplate(1, templatePtr));
    Codecs::FieldInstructionCPtr instruction;
    EXPECT_TRUE(templatePtr->getInstruction(0, instruction));
    return instruction;
  }
}

/// @brief A non-ASCII boolean attribute must be rejected, not passed to
///        toupper as a negative int.
TEST(QuickFAST, testNonAsciiBooleanAttributeIsRejected)
{
  // U+00E9 is two bytes in UTF-8 and the first is 0xC3, so char(0xC3) is
  // negative here and toupper is called outside its domain.
  std::stringstream templateStream(
    "<templates>"
    "  <template name=\"t\" id=\"1\" scp:reset=\"\xC3\xA9\">"
    "    <uInt32 name=\"value\"/>"
    "  </template>"
    "</templates>");
  Codecs::XMLTemplateParser parser;
  EXPECT_THROW((void)parser.parse(templateStream), QuickFAST::TemplateDefinitionError);
}

/// @brief The same attribute, spelled with an ASCII value, still works.
TEST(QuickFAST, testAsciiBooleanAttributeStillParses)
{
  EXPECT_NO_THROW(parse("<uInt32 name=\"value\"><copy/></uInt32>"));
}

/// @brief A pmap index that is not a number must be refused.
TEST(QuickFAST, testNonNumericPmapAttributeIsRejected)
{
  EXPECT_THROW(
    parse("<uInt32 name=\"value\"><copy pmap=\"abc\"/></uInt32>"),
    QuickFAST::TemplateDefinitionError);
}

/// @brief A negative pmap index must be refused, not wrapped to SIZE_MAX.
TEST(QuickFAST, testNegativePmapAttributeIsRejected)
{
  EXPECT_THROW(
    parse("<uInt32 name=\"value\"><copy pmap=\"-1\"/></uInt32>"),
    QuickFAST::TemplateDefinitionError);
}

/// @brief A pmap index with trailing rubbish must be refused.
TEST(QuickFAST, testPmapAttributeWithTrailingGarbageIsRejected)
{
  EXPECT_THROW(
    parse("<uInt32 name=\"value\"><copy pmap=\"3x\"/></uInt32>"),
    QuickFAST::TemplateDefinitionError);
}

/// @brief A well-formed pmap index still parses.
TEST(QuickFAST, testValidPmapAttributeIsAccepted)
{
  EXPECT_NO_THROW(parse("<uInt32 name=\"value\"><copy pmap=\"3\"/></uInt32>"));
}

/// @brief ignore_overflows must reach every integer type, not just int32.
TEST(QuickFAST, testIgnoreOverflowsAppliesToEveryIntegerType)
{
  const char * types[] =
    {"int8", "uInt8", "int16", "uInt16", "int32", "uInt32", "int64", "uInt64"};
  for(const char * type : types)
  {
    const std::string body =
      std::string("<") + type + " name=\"value\" ignore_overflows=\"yes\"/>";
    EXPECT_TRUE(onlyField(parse(body))->getIgnoreOverflow())
      << "ignore_overflows was dropped for " << type;
  }
}

/// @brief Without the attribute, overflow checking stays on.
TEST(QuickFAST, testIgnoreOverflowsDefaultsToChecking)
{
  const char * types[] =
    {"int8", "uInt8", "int16", "uInt16", "int32", "uInt32", "int64", "uInt64"};
  for(const char * type : types)
  {
    const std::string body = std::string("<") + type + " name=\"value\"/>";
    EXPECT_FALSE(onlyField(parse(body))->getIgnoreOverflow())
      << "overflow checking was off by default for " << type;
  }
}

/// @brief An unrecognisable ignore_overflows must be an error, not a silent
///        false.
///
/// The boolean convention here is first-letter, Y/N/T/F, shared with every
/// other boolean attribute, so "ture" is read as true rather than refused.
/// That is not a wonderful answer to a typo, but the alternative is a
/// whole-word check that would reject the "y", "yes", "t" and "true" spellings
/// existing templates rely on. What matters is that a value outside the
/// convention is now refused instead of quietly meaning false.
TEST(QuickFAST, testUnrecognisableIgnoreOverflowsIsRejected)
{
  EXPECT_THROW(
    parse("<int32 name=\"value\" ignore_overflows=\"maybe\"/>"),
    QuickFAST::TemplateDefinitionError);
  EXPECT_TRUE(
    onlyField(parse("<int32 name=\"value\" ignore_overflows=\"true\"/>"))
      ->getIgnoreOverflow());
  EXPECT_FALSE(
    onlyField(parse("<int32 name=\"value\" ignore_overflows=\"no\"/>"))
      ->getIgnoreOverflow());
}
