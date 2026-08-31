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

/// @brief After finalize, sparse/gappy ids still resolve; iteration and post-finalize add work.
TEST(QuickFAST, testTemplateRegistryDenseIdLookup)
{
  std::stringstream xml(
    "<templates>"
    "  <template name=\"t1\" id=\"1\">"
    "    <uInt32 name=\"a\"/>"
    "  </template>"
    "  <template name=\"t100\" id=\"100\">"
    "    <uInt32 name=\"b\"/>"
    "  </template>"
    "  <template name=\"t1000\" id=\"1000\">"
    "    <int32 name=\"c\"/>"
    "  </template>"
    "</templates>");
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr registry = parser.parse(xml);
  ASSERT_TRUE(bool(registry));
  registry->finalize();

  Codecs::TemplateCPtr found;
  EXPECT_TRUE(registry->getTemplate(1, found));
  EXPECT_EQ("t1", found->getTemplateName());
  EXPECT_TRUE(registry->getTemplate(100, found));
  EXPECT_EQ("t100", found->getTemplateName());
  EXPECT_TRUE(registry->getTemplate(1000, found));
  EXPECT_EQ("t1000", found->getTemplateName());
  EXPECT_FALSE(registry->getTemplate(2, found));
  EXPECT_FALSE(registry->getTemplate(999, found));

  size_t walked = 0;
  for(Codecs::TemplateRegistry::const_iterator it = registry->begin();
    it != registry->end(); ++it)
  {
    ++walked;
  }
  EXPECT_EQ(3u, walked);

  EXPECT_TRUE(registry->findNamedTemplate("t100", "", found));
  EXPECT_EQ(100u, found->getId());

  // addTemplate after finalize must still be visible to getTemplate.
  Codecs::TemplatePtr late(new Codecs::Template);
  late->setId(50);
  late->setTemplateName("late");
  registry->addTemplate(late);
  EXPECT_TRUE(registry->getTemplate(50, found));
  EXPECT_EQ("late", found->getTemplateName());
  EXPECT_TRUE(registry->getTemplate(1000, found));
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
