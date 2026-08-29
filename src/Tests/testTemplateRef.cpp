// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Template.h>
#include <Codecs/FieldInstruction.h>
#include <Codecs/DataSourceStream.h>
#include <Codecs/Decoder.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>
//#include <Messages/Field.h>

namespace
{
  const char templatesXML[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<templates xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
      /* Note no id on the following templates: Issue #24 */
    "  <template name=\"TEMPLATE_1\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
    "    <uInt32 name=\"field1\" id=\"A\">\n"
    "      <increment value =\"1\"/>\n"
    "    </uInt32>\n"
    "    <uInt32 name=\"field2\" id=\"B\">\n"
    "      <increment value =\"2\"/>\n"
    "    </uInt32>\n"
    "  </template>\n"
    "  <template name=\"TEMPLATE_2\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
    "    <uInt32 name=\"field1\" id=\"A2\">\n"
    "      <increment value =\"1\"/>\n"
    "    </uInt32>\n"
    "    <uInt32 name=\"field2\" id=\"B2\">\n"
    "      <increment value =\"2\"/>\n"
    "    </uInt32>\n"
    "  </template>\n"
    "  <template name=\"TEMPLATE_10\" id=\"10\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
    "    <uInt32 name=\"field0\" id=\"C\">\n"
    "      <constant value =\"0\"/>\n"
    "    </uInt32>\n"
    "    <templateRef name=\"TEMPLATE_1\"/> <!-- static -->\n"
    "    <uInt32 name=\"field3\" id=\"D\">\n"
    "      <constant value =\"3\"/>\n"
    "    </uInt32>\n"
    "  </template>\n"
    "  <template name=\"TEMPLATE_11\" id=\"11\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
    "    <uInt32 name=\"field0\" id=\"C\">\n"
    "      <constant value =\"0\"/>\n"
    "    </uInt32>\n"
    "    <templateRef name=\"TEMPLATE_2\"/> <!-- static -->\n"
    "    <uInt32 name=\"field3\" id=\"D\">\n"
    "      <constant value =\"3\"/>\n"
    "    </uInt32>\n"
    "  </template>\n"
    "  <template name=\"TEMPLATE_30\" id=\"30\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n"
    "    <uInt32 name=\"field0\" id=\"G\">\n"
    "      <constant value =\"0\"/>\n"
    "    </uInt32>\n"
    "    <uInt32 name=\"field1\" id=\"H\">\n"
    "      <increment value =\"1\"/>\n"
    "    </uInt32>\n"
    "    <uInt32 name=\"field2\" id=\"I\">\n"
    "      <increment value =\"2\"/>\n"
    "    </uInt32>\n"
    "    <uInt32 name=\"field3\" id=\"J\">\n"
    "      <constant value =\"3\"/>\n"
    "    </uInt32>\n"
    "  </template>\n"
    "</templates>\n"
    ;

  const char template10MessageA[] =
    "\xF0\x8A\x8b\x8c";

  const char template10MessageB[] =
    "\xC0\x8A";

  [[maybe_unused]] const char template20MessageA[] =
    "\xC0\x94\xE0\x81\x8b\x8c";

  [[maybe_unused]] const char template20MessageB[] =
    "\xC0\x94\x80\x81";

}

using namespace QuickFAST;
TEST(QuickFAST, testStaticTemplateRefA)
{
  Codecs::XMLTemplateParser parser;
  std::stringstream templateSource(templatesXML);
  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(templateSource);

  EXPECT_TRUE(templateRegistry);
  std::string testString(template10MessageA);
  std::istringstream sourceStream(testString, std::ios::binary);
  Codecs::DataSourceStream source(sourceStream);

  Codecs::Decoder decoder(templateRegistry);

  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);

  decoder.decodeMessage(source, builder);

  Messages::Message & msgOut(consumer.message());

  Messages::FieldCPtr value;
  ASSERT_TRUE(msgOut.getField("field0", value));
  EXPECT_EQ((value->toUInt32()), (0));

  ASSERT_TRUE(msgOut.getField("field1", value));
  EXPECT_EQ((value->toUInt32()), (11));

  ASSERT_TRUE(msgOut.getField("field2", value));
  EXPECT_EQ((value->toUInt32()), (12));

  ASSERT_TRUE(msgOut.getField("field3", value));
  EXPECT_EQ((value->toUInt32()), (3));
}

TEST(QuickFAST, testStaticTemplateRefB)
{
  Codecs::XMLTemplateParser parser;
  std::stringstream templateSource(templatesXML);
  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(templateSource);

  EXPECT_TRUE(templateRegistry);
  std::string testString(template10MessageB);
  std::istringstream sourceStream(testString, std::ios::binary);
  Codecs::DataSourceStream source(sourceStream);

  Codecs::Decoder decoder(templateRegistry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  decoder.decodeMessage(source, builder);

  Messages::Message & msgOut(consumer.message());

  Messages::FieldCPtr value;
  ASSERT_TRUE(msgOut.getField("field0", value));
  EXPECT_EQ((value->toUInt32()), (0));

  ASSERT_TRUE(msgOut.getField("field1", value));
  EXPECT_EQ((value->toUInt32()), (1));

  ASSERT_TRUE(msgOut.getField("field2", value));
  EXPECT_EQ((value->toUInt32()), (2));

  ASSERT_TRUE(msgOut.getField("field3", value));
  EXPECT_EQ((value->toUInt32()), (3));
}
