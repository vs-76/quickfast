// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// MessageFormatter must render sequences and groups, not only scalars — those
// branches are what support dumps use for market-data incrementals.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/Message.h>
#include <Messages/MessageFormatter.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldSequence.h>
#include <Messages/FieldGroup.h>
#include <Messages/Sequence.h>
#include <Messages/FieldSet.h>

using namespace QuickFAST;

namespace
{
  const Messages::FieldIdentity lengthId("NoEntries", "", "268");
  const Messages::FieldIdentity seqId("MDEntries", "", "268");
  const Messages::FieldIdentity groupId("Hdr", "", "49");
  const Messages::FieldIdentity pxId("MDEntryPx", "", "270");
  const Messages::FieldIdentity qtyId("MDEntrySize", "", "271");
}

TEST(QuickFAST, testMessageFormatterFormatsSequenceAndGroup)
{
  Messages::SequencePtr sequence(new Messages::Sequence(lengthId, 1));
  Messages::FieldSetPtr entry(new Messages::FieldSet(2));
  entry->addField(pxId, Messages::FieldInt32::create(100));
  entry->addField(qtyId, Messages::FieldInt32::create(5));
  sequence->addEntry(Messages::FieldSetCPtr(entry));

  Messages::FieldSetPtr group(new Messages::FieldSet(1));
  group->setApplicationType("Header", "fix");
  group->addField(qtyId, Messages::FieldInt32::create(1));

  Messages::Message message(4);
  message.addField(seqId, Messages::FieldSequence::create(Messages::SequenceCPtr(sequence)));
  message.addField(groupId, Messages::FieldGroup::create(Messages::GroupCPtr(group)));

  std::ostringstream out;
  Messages::MessageFormatter formatter(out);
  formatter.formatMessage(message);
  const std::string text = out.str();

  EXPECT_NE(std::string::npos, text.find("Sequence:"));
  EXPECT_NE(std::string::npos, text.find("MDEntries"));
  EXPECT_NE(std::string::npos, text.find("MDEntryPx"));
  EXPECT_NE(std::string::npos, text.find("Group"));
  EXPECT_NE(std::string::npos, text.find("Hdr"));
}
