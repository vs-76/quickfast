// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// The delta and increment operators computed in the field's own type, or in
// int64 regardless of the field's type, and checked nothing.
//
// The FAST specification requires an error when applying a delta leaves the
// field's range. Instead a uInt32 holding 4,000,000,000 with a wire delta of
// 1,000,000,000 produced 705,032,704 in silence, an int64 delta of INT64_MAX
// overflowed on the encoder's nullable adjustment, and an increment field at
// its type maximum wrapped to zero -- which for a sequence number is the
// expected end state of a long-running feed, not an attack.
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
#include <Messages/FieldUInt32.h>
#include <Messages/FieldUInt64.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldInt64.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity valueIdentity("value");

  Codecs::TemplateRegistryPtr registryFor(
    const std::string & fastType,
    const std::string & fieldOperator,
    bool optional = false)
  {
    const std::string xml =
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <typeRef name=\"t\"/>"
      "    <" + fastType + " name=\"value\"" +
             std::string(optional ? " presence=\"optional\"" : "") + ">" +
             fieldOperator +
      "    </" + fastType + ">"
      "  </template>"
      "</templates>";
    std::stringstream templateStream(xml);
    Codecs::XMLTemplateParser parser;
    return parser.parse(templateStream);
  }

  /// @brief Encode a run of values through one encoder, so dictionary state
  /// carries from one message to the next as it does on a real feed.
  std::string encodeRun(
    const Codecs::TemplateRegistryPtr & registry,
    const std::vector<Messages::FieldCPtr> & values)
  {
    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    for(const Messages::FieldCPtr & field : values)
    {
      Messages::Message message(registry->maxFieldCount());
      message.addField(valueIdentity, field);
      encoder.encodeMessage(destination, 1, message);
    }
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  /// @brief Decode a known number of messages, returning the fields seen.
  std::vector<Messages::FieldCPtr> decodeRun(
    const Codecs::TemplateRegistryPtr & registry,
    const std::string & fast,
    size_t messageCount)
  {
    Codecs::DataSourceString source(fast);
    Codecs::Decoder decoder(registry);
    std::vector<Messages::FieldCPtr> values;
    for(size_t message = 0; message < messageCount; ++message)
    {
      Codecs::SingleMessageConsumer consumer;
      Codecs::GenericMessageBuilder builder(consumer);
      decoder.decodeMessage(source, builder);
      Messages::FieldCPtr field;
      if(consumer.message().getField("value", field))
      {
        values.push_back(field);
      }
    }
    return values;
  }
}

/// @brief A delta that leaves a uInt32's range must be reported.
///
/// 4,000,000,000 then 5,000,000,000: the second value is not representable,
/// and the decoder used to deliver 705,032,704 without a word.
TEST(QuickFAST, testUnsignedDeltaOutOfRangeIsReported)
{
  // A uInt32 field cannot hold 5e9, so the offending stream is produced from
  // an int64 template carrying the same two values. Both templates are a
  // single delta field with id 1, so the messages are byte-identical in
  // everything but the value the delta was computed from.
  const std::string fast = encodeRun(registryFor("int64", "<delta/>"), {
    Messages::FieldInt64::create(4000000000LL),
    Messages::FieldInt64::create(5000000000LL)});

  Codecs::TemplateRegistryPtr registry = registryFor("uInt32", "<delta/>");
  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);

  Codecs::SingleMessageConsumer first;
  Codecs::GenericMessageBuilder firstBuilder(first);
  ASSERT_NO_THROW(decoder.decodeMessage(source, firstBuilder));
  Messages::FieldCPtr field;
  ASSERT_TRUE(first.message().getField("value", field));
  ASSERT_EQ(4000000000u, field->toUInt32());

  Codecs::SingleMessageConsumer second;
  Codecs::GenericMessageBuilder secondBuilder(second);
  try
  {
    decoder.decodeMessage(source, secondBuilder);
    Messages::FieldCPtr truncated;
    if(second.message().getField("value", truncated))
    {
      FAIL() << "delta left the field's range and produced "
             << truncated->toUInt32() << " with no error";
    }
  }
  catch(const std::exception &)
  {
    // Reporting the out-of-range delta is what the specification requires.
  }
}

/// @brief An int64 delta at INT64_MAX must not overflow the nullable adjustment.
///
/// The encoder added one to a delta that was already INT64_MAX, which UBSan
/// reports and which made the value arrive as INT64_MIN.
TEST(QuickFAST, testSignedDeltaAtMaximumSurvivesOrIsReported)
{
  // Optional presence is where the overflow lives: the encoder adds one to a
  // non-negative delta to make room for the null encoding, and the delta here
  // is already INT64_MAX.
  Codecs::TemplateRegistryPtr registry = registryFor("int64", "<delta/>", true);

  const int64 maximum = std::numeric_limits<int64>::max();

  // A delta of INT64_MAX plus the null-encoding adjustment does not fit an
  // int64, and the decoder could not carry it either, so reporting is the
  // only honest answer. What is not acceptable is the old one: sending a
  // wrapped delta that arrives as INT64_MIN.
  EXPECT_THROW((void)encodeRun(registry, {
    Messages::FieldInt64::create(maximum)}), EncodingError);

  // One below the maximum leaves room for the adjustment and must round-trip.
  const std::string fast = encodeRun(registry, {
    Messages::FieldInt64::create(maximum - 1)});
  Codecs::DataSourceString source(fast);
  Codecs::Decoder decoder(registry);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  ASSERT_NO_THROW(decoder.decodeMessage(source, builder));

  Messages::FieldCPtr field;
  ASSERT_TRUE(consumer.message().getField("value", field));
  EXPECT_EQ(maximum - 1, field->toInt64());
}

/// @brief A uInt64 above INT64_MAX must not be encoded as a negative delta.
TEST(QuickFAST, testLargeUnsignedDeltaSurvivesOrIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("uInt64", "<delta/>");

  // A jump of nearly 2^64 is expressed as a small negative delta and applied
  // with wraparound. That is how a uInt64 delta field reaches values above
  // INT64_MAX at all, so the range check must not stand in its way: at 64
  // bits the field is as wide as the arithmetic and nothing is lost.
  const uint64 large = std::numeric_limits<uint64>::max() - 1;
  const std::string fast = encodeRun(registry, {
    Messages::FieldUInt64::create(large)});
  const std::vector<Messages::FieldCPtr> values = decodeRun(registry, fast, 1);
  ASSERT_EQ(1u, values.size());
  EXPECT_EQ(large, values[0]->toUInt64());
}

/// @brief Two consecutive large values of opposite sign must not wrap.
///
/// The difference, not either value, is what exceeds the range. This is what
/// makes the encoder's unchecked subtraction reachable from ordinary data.
TEST(QuickFAST, testSignedDeltaBetweenExtremesIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("int64", "<delta/>");

  const int64 maximum = std::numeric_limits<int64>::max();
  const int64 minimum = std::numeric_limits<int64>::min();

  // The difference is 2^64-1, which no int64 delta can carry. Either the
  // encoder reports it or the value round-trips; silently sending a wrapped
  // delta is the one outcome that is not acceptable.
  try
  {
    const std::string fast = encodeRun(registry, {
      Messages::FieldInt64::create(minimum),
      Messages::FieldInt64::create(maximum)});

    Codecs::DataSourceString source(fast);
    Codecs::Decoder decoder(registry);
    std::vector<int64> values;
    for(int i = 0; i < 2; ++i)
    {
      Codecs::SingleMessageConsumer consumer;
      Codecs::GenericMessageBuilder builder(consumer);
      ASSERT_NO_THROW(decoder.decodeMessage(source, builder));
      Messages::FieldCPtr field;
      ASSERT_TRUE(consumer.message().getField("value", field));
      values.push_back(field->toInt64());
    }
    EXPECT_EQ(minimum, values[0]);
    EXPECT_EQ(maximum, values[1]) << "delta wrapped silently";
  }
  catch(const EncodingError &)
  {
    // Reporting the unrepresentable delta is the other acceptable answer.
  }
}

/// @brief An increment field at its type maximum must be reported, not wrapped.
///
/// The increment operator exists for monotonically rising fields, so a uInt32
/// sequence number reaching 4,294,967,295 is where a long-running feed ends
/// up. It used to continue at zero.
TEST(QuickFAST, testIncrementAtMaximumIsReported)
{
  Codecs::TemplateRegistryPtr registry = registryFor("uInt32", "<increment/>");

  const uint32 maximum = std::numeric_limits<uint32>::max();
  // Send the maximum, then omit the field so the decoder increments it.
  Codecs::Encoder encoder(registry);
  Codecs::DataDestination destination;
  Messages::Message message(registry->maxFieldCount());
  message.addField(valueIdentity, Messages::FieldUInt32::create(maximum));
  encoder.encodeMessage(destination, 1, message);
  std::string fast;
  destination.toString(fast);

  // A second message with the pmap bit clear tells the decoder to increment.
  const std::string incrementing = fast + std::string("\x80\x81", 2);

  Codecs::DataSourceString source(incrementing);
  Codecs::Decoder decoder(registry);

  Codecs::SingleMessageConsumer first;
  Codecs::GenericMessageBuilder firstBuilder(first);
  ASSERT_NO_THROW(decoder.decodeMessage(source, firstBuilder));
  Messages::FieldCPtr field;
  ASSERT_TRUE(first.message().getField("value", field));
  ASSERT_EQ(maximum, field->toUInt32());

  Codecs::SingleMessageConsumer second;
  Codecs::GenericMessageBuilder secondBuilder(second);
  try
  {
    decoder.decodeMessage(source, secondBuilder);
    Messages::FieldCPtr wrapped;
    if(second.message().getField("value", wrapped))
    {
      FAIL() << "increment wrapped to " << wrapped->toUInt32()
             << " instead of reporting overflow";
    }
  }
  catch(const std::exception &)
  {
    // Reporting the overflow is the expected answer.
  }
}

/// @brief Ordinary delta and increment traffic must be unaffected.
TEST(QuickFAST, testDeltaAndIncrementInRangeAreUnaffected)
{
  Codecs::TemplateRegistryPtr deltaRegistry = registryFor("uInt32", "<delta/>");
  const std::string deltaFast = encodeRun(deltaRegistry, {
    Messages::FieldUInt32::create(100),
    Messages::FieldUInt32::create(150),
    Messages::FieldUInt32::create(50),
    Messages::FieldUInt32::create(0)});
  const std::vector<Messages::FieldCPtr> deltaValues =
    decodeRun(deltaRegistry, deltaFast, 4);
  ASSERT_EQ(4u, deltaValues.size());
  EXPECT_EQ(100u, deltaValues[0]->toUInt32());
  EXPECT_EQ(150u, deltaValues[1]->toUInt32());
  EXPECT_EQ(50u, deltaValues[2]->toUInt32());
  EXPECT_EQ(0u, deltaValues[3]->toUInt32());

  Codecs::TemplateRegistryPtr incrementRegistry =
    registryFor("uInt32", "<increment/>");
  const std::string incrementFast = encodeRun(incrementRegistry, {
    Messages::FieldUInt32::create(7),
    Messages::FieldUInt32::create(8),
    Messages::FieldUInt32::create(9),
    Messages::FieldUInt32::create(100)});
  const std::vector<Messages::FieldCPtr> incrementValues =
    decodeRun(incrementRegistry, incrementFast, 4);
  ASSERT_EQ(4u, incrementValues.size());
  EXPECT_EQ(7u, incrementValues[0]->toUInt32());
  EXPECT_EQ(8u, incrementValues[1]->toUInt32());
  EXPECT_EQ(9u, incrementValues[2]->toUInt32());
  EXPECT_EQ(100u, incrementValues[3]->toUInt32());
}
