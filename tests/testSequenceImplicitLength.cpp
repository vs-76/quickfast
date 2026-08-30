// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <sstream>

#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceBuffer.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldSequence.h>
#include <Messages/FieldUInt32.h>
#include <Messages/MessageFormatter.h>
#include <Messages/Sequence.h>

using namespace QuickFAST;

namespace
{
  // FAST v1.1 makes <length> optional inside <sequence>.  When it is omitted
  // the decoder synthesises an implicit uInt32 length instruction, and the
  // decoded Messages::Sequence keeps that instruction's identity by reference
  // for its whole lifetime.
  const char implicitLengthTemplate[] = "\n\
<templates>\n\
  <template id=\"1\" name=\"implicitLength\">\n\
    <typeRef name=\"implicitLength\"/>\n\
    <sequence name=\"entries\">\n\
      <uInt32 name=\"value\"/>\n\
    </sequence>\n\
  </template>\n\
</templates>\n\
";

  const Messages::FieldIdentity entriesIdentity("entries");
  const Messages::FieldIdentity valueIdentity("value");
  const Messages::FieldIdentity encodeLengthIdentity("entriesLength");
}

TEST(QuickFAST, testSequenceWithoutLengthInstruction)
{
  std::stringstream templateStream(implicitLengthTemplate);
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr templateRegistry = parser.parse(templateStream);

  Messages::SequencePtr entries(new Messages::Sequence(encodeLengthIdentity, 2));
  for(uint32 nEntry = 0; nEntry < 2; ++nEntry)
  {
    Messages::FieldSetPtr entry(new Messages::FieldSet(1));
    entry->addField(valueIdentity, Messages::FieldUInt32::create(nEntry + 10));
    entries->addEntry(entry);
  }

  Messages::Message msg(templateRegistry->maxFieldCount());
  msg.addField(entriesIdentity, Messages::FieldSequence::create(entries));

  Codecs::Encoder encoder(templateRegistry);
  Codecs::DataDestination destination;
  encoder.encodeMessage(destination, 1, msg);
  WorkingBuffer fastMsg;
  destination.toWorkingBuffer(fastMsg);
  destination.clear();

  Codecs::Decoder decoder(templateRegistry);
  Codecs::DataSourceBuffer source(fastMsg.begin(), fastMsg.size());
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  decoder.decodeMessage(source, builder);
  Messages::Message & decoded = consumer.message();

  Messages::FieldCPtr field;
  ASSERT_TRUE(decoded.getField("entries", field));
  Messages::SequenceCPtr sequence = field->toSequence();
  ASSERT_TRUE(bool(sequence));
  EXPECT_EQ((sequence->size()), (2u));

  // Messages::Sequence keeps this identity by reference.  Reading it after the
  // decode call has returned is a stack-use-after-return unless the implicit
  // length instruction outlives the decoder; run this under -fsanitize=address
  // to see the difference.
  const Messages::FieldIdentity & lengthIdentity = sequence->getLengthIdentity();
  EXPECT_FALSE(lengthIdentity.name().empty());
  EXPECT_EQ((lengthIdentity.id()), (""));

  // A second decode from the same registry must report the same identity: the
  // instruction belongs to the template, not to the decode call.
  Codecs::DataSourceBuffer source2(fastMsg.begin(), fastMsg.size());
  Codecs::SingleMessageConsumer consumer2;
  Codecs::GenericMessageBuilder builder2(consumer2);
  decoder.reset();
  decoder.decodeMessage(source2, builder2);
  Messages::FieldCPtr field2;
  ASSERT_TRUE(consumer2.message().getField("entries", field2));
  EXPECT_EQ(
    (field2->toSequence()->getLengthIdentity().name()),
    (lengthIdentity.name()));

  // MessageFormatter walks the same reference.
  std::ostringstream formatted;
  Messages::MessageFormatter formatter(formatted);
  formatter.formatMessage(decoded);
  EXPECT_NE((formatted.str().find("entries")), (std::string::npos));
}
