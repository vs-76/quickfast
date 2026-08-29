// Copyright (c) 2026, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A group whose application type matches its parent's is folded into the
// parent: its fields are decoded straight into the enclosing field set and the
// group vanishes from the decoded message. That is deliberate, and the reason
// given in FieldInstructionGroup.cpp is a good one -- a group is an artifact of
// the template, and the same message encoded with different templates could
// carry different sets of fields in groups.
//
// The consequence is less obviously deliberate. typeRef looks like an
// annotation, so adding one to a group to document its application type reads
// like a comment. It is not: it is what makes the two types differ, so it
// decides whether a consumer reads msg["a"] or msg["grp"]["a"]. Nothing on
// either side reports anything; the field is simply not where it was.
//
// Nothing here is a bug to fix, so this file exists to pin both behaviours so
// that neither changes by accident.
#include <Common/QuickFASTPch.h>
#include <gtest/gtest.h>

#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldGroup.h>
#include <Messages/Group.h>

using namespace QuickFAST;

namespace
{
  // FieldSet keeps a reference to the identity, so these must outlive the
  // message rather than being temporaries at the call site.
  const Messages::FieldIdentity aIdentity("a");
  const Messages::FieldIdentity bIdentity("b");
  const Messages::FieldIdentity groupIdentity("grp");

  /// @brief Round-trip a two-field message through the supplied template.
  ///
  /// Encoding rather than hand-writing the bytes, because the wire layout is
  /// exactly what the typeRef changes and a hand-written stream would be
  /// asserting the layout this test is trying not to depend on.
  void roundTrip(const std::string & templateText, Codecs::SingleMessageConsumer & consumer)
  {
    std::stringstream templates(templateText);
    Codecs::XMLTemplateParser parser;
    Codecs::TemplateRegistryPtr registry = parser.parse(templates);

    // The encoder always wants the group as a group; folding is a decode-side
    // decision only, which is itself part of the asymmetry being pinned here.
    Messages::GroupPtr group(new Messages::Group(1));
    group->addField(bIdentity, Messages::FieldUInt32::create(3));

    Messages::Message message(registry->maxFieldCount());
    message.addField(aIdentity, Messages::FieldUInt32::create(2));
    message.addField(groupIdentity, Messages::FieldGroup::create(group));

    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, 1, message);
    std::string fast;
    destination.toString(fast);

    Codecs::DataSourceString source(fast);
    Codecs::Decoder decoder(registry);
    Codecs::GenericMessageBuilder builder(consumer);
    decoder.decodeMessage(source, builder);
  }

  const char * withoutTypeRef =
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <uInt32 name=\"a\"/>"
    "    <group name=\"grp\">"
    "      <uInt32 name=\"b\"/>"
    "    </group>"
    "  </template>"
    "</templates>";

  const char * withTypeRef =
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"MessageType\"/>"
    "    <uInt32 name=\"a\"/>"
    "    <group name=\"grp\">"
    "      <typeRef name=\"GroupType\"/>"
    "      <uInt32 name=\"b\"/>"
    "    </group>"
    "  </template>"
    "</templates>";
}

/// @brief Without a typeRef the group folds into its parent.
TEST(QuickFAST, testGroupWithoutTypeRefFoldsIntoTheParent)
{
  Codecs::SingleMessageConsumer consumer;
  roundTrip(withoutTypeRef, consumer);
  const Messages::Message & message = consumer.message();

  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("a", field));
  EXPECT_EQ(2u, field->toUInt32());

  // The group is gone: its field is a direct member of the message.
  ASSERT_TRUE(message.getField("b", field));
  EXPECT_EQ(3u, field->toUInt32());
  EXPECT_FALSE(message.getField("grp", field));
}

/// @brief Adding a typeRef makes the group appear in the decoded message.
///
/// This is the shape change the finding is about: msg["b"] becomes
/// msg["grp"]["b"], and the only edit was one line of apparent annotation.
TEST(QuickFAST, testGroupWithTypeRefAppearsInTheMessage)
{
  Codecs::SingleMessageConsumer consumer;
  roundTrip(withTypeRef, consumer);
  const Messages::Message & message = consumer.message();

  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("a", field));
  EXPECT_EQ(2u, field->toUInt32());

  // The field that used to be reachable by name at the top level is not.
  EXPECT_FALSE(message.getField("b", field));

  ASSERT_TRUE(message.getField("grp", field));
  Messages::GroupCPtr group = field->toGroup();
  ASSERT_TRUE(bool(group));
  EXPECT_EQ(std::string("GroupType"), group->getApplicationType());

  Messages::FieldCPtr inner;
  ASSERT_TRUE(group->getField("b", inner));
  EXPECT_EQ(3u, inner->toUInt32());
}

/// @brief A typeRef matching the parent's folds just as an absent one does.
///
/// It is the comparison that decides, not the presence of the element, so a
/// typeRef that restates the enclosing type changes nothing.
TEST(QuickFAST, testGroupTypeRefMatchingTheParentStillFolds)
{
  Codecs::SingleMessageConsumer consumer;
  roundTrip(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"SharedType\"/>"
    "    <uInt32 name=\"a\"/>"
    "    <group name=\"grp\">"
    "      <typeRef name=\"SharedType\"/>"
    "      <uInt32 name=\"b\"/>"
    "    </group>"
    "  </template>"
    "</templates>",
    consumer);
  const Messages::Message & message = consumer.message();

  Messages::FieldCPtr field;
  ASSERT_TRUE(message.getField("b", field));
  EXPECT_EQ(3u, field->toUInt32());
  EXPECT_FALSE(message.getField("grp", field));
}

/// @brief A typeRef on the *template* un-folds a group that has none.
///
/// A group without a typeRef does not inherit its parent's application type,
/// so the comparison is between the template's declared type and the group's
/// default. Annotating the message therefore restructures it just as
/// annotating the group does, one level further away from the group that
/// moves.
TEST(QuickFAST, testTemplateTypeRefAlsoUnfoldsAnUnannotatedGroup)
{
  Codecs::SingleMessageConsumer consumer;
  roundTrip(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"MessageType\"/>"
    "    <uInt32 name=\"a\"/>"
    "    <group name=\"grp\">"
    "      <uInt32 name=\"b\"/>"
    "    </group>"
    "  </template>"
    "</templates>",
    consumer);
  const Messages::Message & message = consumer.message();

  EXPECT_EQ(std::string("MessageType"), message.getApplicationType());

  Messages::FieldCPtr field;
  EXPECT_FALSE(message.getField("b", field));
  ASSERT_TRUE(message.getField("grp", field));

  Messages::GroupCPtr group = field->toGroup();
  ASSERT_TRUE(bool(group));
  Messages::FieldCPtr inner;
  ASSERT_TRUE(group->getField("b", inner));
  EXPECT_EQ(3u, inner->toUInt32());
}

/// @brief A group typeRef un-folds even when the template declares none.
TEST(QuickFAST, testGroupTypeRefUnfoldsWithoutATemplateTypeRef)
{
  Codecs::SingleMessageConsumer consumer;
  roundTrip(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <uInt32 name=\"a\"/>"
    "    <group name=\"grp\">"
    "      <typeRef name=\"GroupType\"/>"
    "      <uInt32 name=\"b\"/>"
    "    </group>"
    "  </template>"
    "</templates>",
    consumer);
  const Messages::Message & message = consumer.message();

  Messages::FieldCPtr field;
  EXPECT_FALSE(message.getField("b", field));
  ASSERT_TRUE(message.getField("grp", field));
  EXPECT_EQ(std::string("GroupType"), field->toGroup()->getApplicationType());
}
