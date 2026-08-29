// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A misplaced tag in a template file used to kill the process silently.
//
// Nearly every element handler starts with schemaElements_.top(), which is
// undefined on an empty stack. Two independent conditions made that reachable:
// the sentinel push that would have kept the stack non-empty is commented out,
// and the Xerces reader has core validation disabled, so nothing rejects a
// field or op appearing outside a <template>. A release build took the
// out-of-bounds deque access and then made a virtual call through a garbage
// shared_ptr. Callers wrap parse() in catch(const std::exception &), but a
// segfault is not an exception, so the process died with no diagnostic at all.
//
// Template files are hand-maintained, so a tag indented one level too far out
// is an ordinary editing mistake.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  void expectRejected(const std::string & xml)
  {
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    EXPECT_THROW((void)parser.parse(templateStream), TemplateDefinitionError);
  }
}

/// @brief A field operator outside any field must be reported, not crash.
TEST(QuickFAST, testStrayOperatorIsRejected)
{
  expectRejected("<templates><nop/></templates>");
  expectRejected("<templates><copy/></templates>");
  expectRejected("<templates><constant value=\"1\"/></templates>");
}

/// @brief A typeRef outside any template must be reported, not crash.
TEST(QuickFAST, testStrayTypeRefIsRejected)
{
  expectRejected("<templates><typeRef name=\"Stray\"/></templates>");
}

/// @brief A field outside any template must be reported, not crash.
TEST(QuickFAST, testStrayFieldIsRejected)
{
  expectRejected("<templates><uInt32 name=\"Stray\"/></templates>");
  expectRejected("<templates><string name=\"Stray\"/></templates>");
  expectRejected("<templates><decimal name=\"Stray\"/></templates>");
  expectRejected("<templates><sequence name=\"Stray\"/></templates>");
  expectRejected("<templates><group name=\"Stray\"/></templates>");
}

/// @brief A field as the root element must be reported, not crash.
TEST(QuickFAST, testFieldAsRootElementIsRejected)
{
  expectRejected("<uInt32 name=\"Stray\"/>");
}

/// @brief A well-formed template still parses.
TEST(QuickFAST, testValidTemplateStillParses)
{
  const std::string xml =
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"t\"/>"
    "    <uInt32 name=\"value\"><copy/></uInt32>"
    "    <decimal name=\"price\"><mantissa><delta/></mantissa></decimal>"
    "    <sequence name=\"legs\">"
    "      <length name=\"legCount\"/>"
    "      <string name=\"leg\"><tail/></string>"
    "    </sequence>"
    "    <group name=\"g\"><uInt32 name=\"inner\"><nop/></uInt32></group>"
    "  </template>"
    "</templates>";
  std::stringstream templateStream(xml);
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr registry;
  ASSERT_NO_THROW(registry = parser.parse(templateStream));
  ASSERT_TRUE(!!registry);
  EXPECT_EQ(1u, registry->size());
}
