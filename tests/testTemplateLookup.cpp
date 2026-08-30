// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Looking up a template that is not there must report absence, not invent one.
// Misses are how a decoder decides the wire template id is unknown.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Template.h>

using namespace QuickFAST;

namespace
{
  Codecs::TemplateRegistryPtr oneTemplate()
  {
    std::stringstream xml(
      "<templates>"
      "  <template name=\"known\" id=\"7\">"
      "    <uInt32 name=\"value\"/>"
      "  </template>"
      "</templates>");
    Codecs::XMLTemplateParser parser;
    return parser.parse(xml);
  }
}

/// @brief getTemplate / findNamedTemplate return false for unknown ids and names.
TEST(QuickFAST, testTemplateRegistryMissLookups)
{
  Codecs::TemplateRegistryPtr registry = oneTemplate();

  Codecs::TemplateCPtr found;
  EXPECT_TRUE(registry->getTemplate(7, found));
  ASSERT_TRUE(bool(found));
  EXPECT_EQ(7u, found->getId());
  EXPECT_EQ("known", found->getTemplateName());

  Codecs::TemplateCPtr missing;
  EXPECT_FALSE(registry->getTemplate(99, missing));
  EXPECT_FALSE(registry->findNamedTemplate("nope", "", missing));
  EXPECT_FALSE(registry->findNamedTemplate("known", "otherns", missing));

  Codecs::TemplateCPtr byName;
  EXPECT_TRUE(registry->findNamedTemplate("known", "", byName));
  EXPECT_EQ(7u, byName->getId());
}

/// @brief Template reset/ignore flags are readable after parse defaults.
TEST(QuickFAST, testTemplateFlagsDefaultOff)
{
  Codecs::TemplateRegistryPtr registry = oneTemplate();
  Codecs::TemplateCPtr found;
  ASSERT_TRUE(registry->getTemplate(7, found));
  EXPECT_FALSE(found->getReset());
  EXPECT_FALSE(found->getIgnore());
}
