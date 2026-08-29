// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Four things about the FieldSet accessors.
//
// operator[] and getFieldInfo sit six lines apart over the same array; one
// checks its index and the other does not. Past used_ those slots hold the raw
// bytes of the unsigned char[] allocation, so getFieldInfo dereferences a
// garbage FieldCPtr.
//
// getSequenceEntry hands its index straight to std::vector::operator[]. Nothing
// inside FieldSet can reach it out of range, because the encoder takes the
// count from getSequenceLength over the same vector, but a consumer's own
// MessageAccessor whose length disagrees with its entry count gets undefined
// behaviour rather than a decode error.
//
// replaceField keeps scanning after an identity match on an undefined field,
// so with a duplicate identity it replaces a later entry -- the opposite of the
// first-match rule getField and isPresent implement over the same array.
//
// And the application type is never checked by a round trip, because equals
// skips the comparison when either side is "any" and Message's constructor
// makes the hand-built side "any" every time.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/FieldSet.h>
#include <Messages/Message.h>
#include <Messages/Sequence.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldSequence.h>
#include <Messages/FieldIdentity.h>

#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

/// @brief getFieldInfo must range check its index, as operator[] does.
TEST(QuickFAST, testGetFieldInfoIsRangeChecked)
{
  const Messages::FieldIdentity identity("value");
  Messages::FieldSet fields(4);
  fields.addField(identity, Messages::FieldUInt32::create(7));

  std::string name;
  ValueType::Type type = ValueType::UNDEFINED;
  Messages::FieldCPtr field;

  EXPECT_NO_THROW(fields.getFieldInfo(0, name, type, field));
  EXPECT_EQ("value", name);
  EXPECT_THROW(fields.getFieldInfo(1, name, type, field), UsageError);
  EXPECT_THROW((void)fields[1], UsageError);
}

/// @brief An out of range sequence entry must be refused, not indexed.
TEST(QuickFAST, testGetSequenceEntryIsRangeChecked)
{
  const Messages::FieldIdentity sequenceIdentity("s");
  const Messages::FieldIdentity lengthIdentity("len");
  const Messages::FieldIdentity valueIdentity("value");

  Messages::SequencePtr sequence(new Messages::Sequence(lengthIdentity, 1));
  Messages::FieldSetPtr entry(new Messages::FieldSet(1));
  entry->addField(valueIdentity, Messages::FieldUInt32::create(7));
  sequence->addEntry(entry);

  Messages::FieldSet fields(4);
  fields.addField(sequenceIdentity, Messages::FieldSequence::create(sequence));

  size_t length = 0;
  ASSERT_TRUE(fields.getSequenceLength(sequenceIdentity, length));
  EXPECT_EQ(1u, length);

  const Messages::MessageAccessor * accessor = 0;
  EXPECT_TRUE(fields.getSequenceEntry(sequenceIdentity, 0, accessor));
  EXPECT_TRUE(accessor != 0);
  EXPECT_FALSE(fields.getSequenceEntry(sequenceIdentity, 1, accessor));
}

/// @brief replaceField must stop at the first matching identity.
TEST(QuickFAST, testReplaceFieldStopsAtTheFirstMatch)
{
  const Messages::FieldIdentity identity("value");
  Messages::FieldSet fields(4);
  // The first entry carries the identity but is absent, the second is present.
  fields.addField(identity, Messages::FieldUInt32::createNull());
  fields.addField(identity, Messages::FieldUInt32::create(7));

  EXPECT_FALSE(fields.replaceField(identity, Messages::FieldUInt32::create(9)));

  // The later entry must be untouched: scanning past the first match would
  // have replaced it, which is not what getField and isPresent do.
  Messages::FieldCPtr field;
  ASSERT_TRUE(fields[1].getField() != 0);
  EXPECT_EQ(7u, fields[1].getField()->toUInt32());
}

/// @brief Replacing a present field still works.
TEST(QuickFAST, testReplaceFieldReplacesAPresentField)
{
  const Messages::FieldIdentity identity("value");
  Messages::FieldSet fields(4);
  fields.addField(identity, Messages::FieldUInt32::create(7));

  EXPECT_TRUE(fields.replaceField(identity, Messages::FieldUInt32::create(9)));

  Messages::FieldCPtr field;
  ASSERT_TRUE(fields.getField(identity, field));
  EXPECT_EQ(9u, field->toUInt32());
}

/// @brief The application type must survive a round trip.
///
/// No round-trip test checks this, because equals skips the comparison when
/// either side is "any" and Message's constructor makes the hand-built side
/// "any" every time. Checking the decoded side directly closes the gap
/// without changing what the wildcard means.
TEST(QuickFAST, testApplicationTypeSurvivesARoundTrip)
{
  std::stringstream templates(
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"Quote\" ns=\"urn:example\"/>"
    "    <uInt32 name=\"value\"/>"
    "  </template>"
    "</templates>");
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr registry = parser.parse(templates);

  const Messages::FieldIdentity valueIdentity("value");
  Messages::Message message(registry->maxFieldCount());
  message.addField(valueIdentity, Messages::FieldUInt32::create(7));

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

  EXPECT_EQ("Quote", consumer.message().getApplicationType());
  EXPECT_EQ("urn:example", consumer.message().getApplicationTypeNs());
}

/// @brief equals compares application types when neither side is the wildcard.
TEST(QuickFAST, testEqualsComparesRealApplicationTypes)
{
  const Messages::FieldIdentity identity("value");

  Messages::FieldSet left(2);
  left.setApplicationType("Quote", "urn:example");
  left.addField(identity, Messages::FieldUInt32::create(7));

  Messages::FieldSet right(2);
  right.setApplicationType("Trade", "urn:example");
  right.addField(identity, Messages::FieldUInt32::create(7));

  std::stringstream reason;
  EXPECT_FALSE(left.equals(right, reason));
  EXPECT_NE(std::string::npos, reason.str().find("Quote"));

  Messages::FieldSet same(2);
  same.setApplicationType("Quote", "urn:example");
  same.addField(identity, Messages::FieldUInt32::create(7));
  std::stringstream ignored;
  EXPECT_TRUE(left.equals(same, ignored)) << ignored.str();
}
