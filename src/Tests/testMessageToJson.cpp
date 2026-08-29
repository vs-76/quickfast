// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//

#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/MessageToJson.h>
#include <Messages/Message.h>
#include <Messages/Sequence.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldByteVector.h>
#include <Messages/FieldDecimal.h>
#include <Messages/FieldGroup.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldSequence.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldUInt64.h>
#include <Messages/FieldUtf8.h>
#include <Common/Decimal.h>

#include <sstream>

using namespace QuickFAST;
using namespace Messages;

namespace
{
  std::string toJson(const FieldSet & fields, const JsonOptions & options = JsonOptions())
  {
    std::ostringstream oss;
    MessageToJson(options).formatFieldSet(fields, oss);
    return oss.str();
  }
}

TEST(MessageToJson, EmptyObject)
{
  FieldSet fields(0);
  EXPECT_EQ("{}", toJson(fields));
}

TEST(MessageToJson, ScalarsByName)
{
  FieldSet fields(8);
  fields.setApplicationType("MDIncRefresh", "fix");
  static const FieldIdentity msgType("MsgType", "", "35");
  static const FieldIdentity size("MDEntrySize", "", "271");
  static const FieldIdentity px("MDEntryPx", "", "270");
  static const FieldIdentity big("BigUInt", "", "999");
  static const FieldIdentity text("Text", "", "58");

  fields.addField(msgType, FieldAscii::create("X"));
  fields.addField(size, FieldUInt32::create(10));
  fields.addField(px, FieldDecimal::create(Decimal(12345, -2)));
  fields.addField(big, FieldUInt64::create(uint64(1) << 60));
  fields.addField(text, FieldUtf8::create("a\"b\\c"));

  EXPECT_EQ(
    "{\"applicationType\":\"MDIncRefresh\",\"applicationTypeNs\":\"fix\","
    "\"MsgType\":\"X\",\"MDEntrySize\":10,\"MDEntryPx\":\"123.45\","
    "\"BigUInt\":\"1152921504606846976\",\"Text\":\"a\\\"b\\\\c\"}",
    toJson(fields));
}

TEST(MessageToJson, ScalarsById)
{
  FieldSet fields(4);
  static const FieldIdentity msgType("MsgType", "", "35");
  static const FieldIdentity size("MDEntrySize", "", "271");
  fields.addField(msgType, FieldAscii::create("X"));
  fields.addField(size, FieldUInt32::create(10));

  JsonOptions options;
  options.keyMode = JsonOptions::KeyMode::Id;
  options.includeApplicationType = false;

  EXPECT_EQ("{\"35\":\"X\",\"271\":10}", toJson(fields, options));
}

TEST(MessageToJson, IdFallsBackToNameWhenEmpty)
{
  FieldSet fields(2);
  static const FieldIdentity anon("AnonField");
  fields.addField(anon, FieldInt32::create(-7));

  JsonOptions options;
  options.keyMode = JsonOptions::KeyMode::Id;
  options.includeApplicationType = false;

  EXPECT_EQ("{\"AnonField\":-7}", toJson(fields, options));
}

TEST(MessageToJson, SequenceAndGroup)
{
  static const FieldIdentity entries("MDEntries", "", "268");
  static const FieldIdentity length("NoMDEntries", "", "268");
  static const FieldIdentity entryType("MDEntryType", "", "269");
  static const FieldIdentity nested("Extra", "", "900");
  static const FieldIdentity nestedValue("Flag", "", "901");

  FieldSetPtr entry(new FieldSet(4));
  entry->addField(entryType, FieldAscii::create("0"));

  FieldSetPtr group(new FieldSet(2));
  group->setApplicationType("ExtraGroup", "");
  group->addField(nestedValue, FieldUInt32::create(1));
  entry->addField(nested, FieldGroup::create(group));

  SequencePtr sequence(new Sequence(length, 1));
  sequence->addEntry(entry);

  FieldSet fields(2);
  fields.addField(entries, FieldSequence::create(sequence));

  JsonOptions options;
  options.includeApplicationType = true;

  EXPECT_EQ(
    "{\"MDEntries\":[{\"MDEntryType\":\"0\","
    "\"Extra\":{\"applicationType\":\"ExtraGroup\",\"Flag\":1}}]}",
    toJson(fields, options));
}

TEST(MessageToJson, EmptySequence)
{
  static const FieldIdentity entries("MDEntries", "", "268");
  static const FieldIdentity length("NoMDEntries", "", "268");
  SequencePtr sequence(new Sequence(length, 0));

  FieldSet fields(1);
  fields.addField(entries, FieldSequence::create(sequence));

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"MDEntries\":[]}", toJson(fields, options));
}

TEST(MessageToJson, ByteVectorBase64AndHex)
{
  static const FieldIdentity raw("Raw", "", "96");
  const unsigned char bytes[] = {0x48, 0x69, 0x21}; // "Hi!"

  FieldSet fields(1);
  fields.addField(raw, FieldByteVector::create(bytes, 3));

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"Raw\":\"SGkh\"}", toJson(fields, options));

  options.byteVectors = JsonOptions::ByteVectorEncoding::Hex;
  EXPECT_EQ("{\"Raw\":\"486921\"}", toJson(fields, options));
}

TEST(MessageToJson, SkipsUndefinedFields)
{
  static const FieldIdentity present("Present", "", "1");
  static const FieldIdentity absent("Absent", "", "2");

  FieldSet fields(2);
  fields.addField(present, FieldUInt32::create(1));
  fields.addField(absent, FieldUInt32::createNull());

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"Present\":1}", toJson(fields, options));
}

TEST(MessageToJson, ControlCharacterEscaping)
{
  static const FieldIdentity text("Text", "", "58");
  FieldSet fields(1);
  fields.addField(text, FieldAscii::create(std::string("a\nb\tc\x01")));

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"Text\":\"a\\nb\\tc\\u0001\"}", toJson(fields, options));
}

TEST(MessageToJson, DecimalExactBeyondDouble)
{
  // Mantissa past the 53-bit float significand must stay exact as a string.
  static const FieldIdentity px("Px", "", "270");
  FieldSet fields(1);
  fields.addField(px, FieldDecimal::create(Decimal(9007199254740993LL, -2)));

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"Px\":\"90071992547409.93\"}", toJson(fields, options));
}

TEST(MessageToJson, DecimalSmallFraction)
{
  static const FieldIdentity px("Px", "", "270");
  FieldSet fields(1);
  fields.addField(px, FieldDecimal::create(Decimal(5, -3)));

  JsonOptions options;
  options.includeApplicationType = false;
  EXPECT_EQ("{\"Px\":\"0.005\"}", toJson(fields, options));
}
