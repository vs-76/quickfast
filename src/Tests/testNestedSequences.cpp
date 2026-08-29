// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Decoder.h>
#include <Codecs/Encoder.h>
#include <Messages/FieldDecimal.h>
#include <Messages/FieldSequence.h>
#include <Messages/Sequence.h>
#include <Messages/FieldIdentity.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>

using namespace QuickFAST;

namespace
{
  const char * templates =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
    "<templates xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">"
    "    <template name=\"Test1Message\" id=\"5000\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">"
    "        <sequence name=\"Parties\">"
    "            <length name=\"NoParties\" id=\"9998\"/>"
    "            <sequence name=\"Entries\">"
    "                <length name=\"NoEntries\" id=\"9997\"/>"
    "                <decimal name=\"Price\" id=\"80\"><copy/></decimal>"
    "            </sequence>"
    "        </sequence>"
    "    </template>"
    "    <template name=\"Test2Message\" id=\"4000\" xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">"
    "        <sequence name=\"Entries\">"
    "            <length name=\"NoEntries\" id=\"8997\"/>"
    "            <decimal name=\"Price\" id=\"70\"><copy/></decimal>"
    "        </sequence>"
    "    </template>"
    "</templates>"
    ;

}

namespace{
  Messages::FieldIdentity noParties("NoParties");
  Messages::FieldIdentity noEntries("NoEntries");

  Messages::FieldIdentity priceIdentity("Price");
  Messages::FieldIdentity entriesIdentity("Entries");
  Messages::FieldIdentity partiesIdentity("Parties");

}

TEST(QuickFAST, testNestedSequences)
{
  SCOPED_TRACE("");
  std::stringstream templateStream(templates);
  Codecs::XMLTemplateParser parser;
  Codecs::TemplateRegistryPtr templateRegistry = parser.parse(templateStream);
//  templateRegistry->display(std::cout);

  Codecs::Encoder enc(templateRegistry);
  Codecs::Decoder dec(templateRegistry);

  Messages::FieldSetPtr msg(new Messages::FieldSet(20));
  Messages::SequencePtr parties(new Messages::Sequence(noParties, 20));
  Messages::FieldSetPtr p1(new Messages::FieldSet(20));
  Messages::SequencePtr entries(new Messages::Sequence(noEntries, 20));
  Messages::FieldSetPtr e1(new Messages::FieldSet(20));

  e1->addField( priceIdentity,
    Messages::FieldDecimal::create(Decimal(1, -1)) ); //(1, 0) is ok

  entries->addEntry(e1);
  entries->addEntry(e1);
  entries->addEntry(e1);

  p1->addField( entriesIdentity,
    Messages::FieldSequence::create(entries) );

  parties->addEntry(p1);
  parties->addEntry(p1);
  parties->addEntry(p1);

  msg->addField(partiesIdentity,
    Messages::FieldSequence::create(parties) );

  Codecs::TemplateCPtr tpl;

  ASSERT_TRUE(enc.findTemplate("Test1Message", "", tpl));
  Codecs::DataDestination dest;
  enc.encodeMessage(dest, tpl->getId(), *msg);
//  std::cout << "encoded" << std::endl;

  std::string encodedValue;
  dest.toString(encodedValue);
  Codecs::DataSourceString src(encodedValue);
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  dec.decodeMessage(src, builder);
//  std::cout << "decoded" << std::endl;

  Messages::Message & message = consumer.message();

  Messages::FieldCPtr decodedParties;
  message.getField("Parties", decodedParties);
  ASSERT_TRUE(decodedParties);
  Messages::SequenceCPtr decodedPartiesSequence = decodedParties->toSequence();
  ASSERT_TRUE(decodedPartiesSequence);

  Messages::FieldSetCPtr decodedPartyItem = (*decodedPartiesSequence)[0];
  ASSERT_TRUE(decodedPartyItem);

  // now get the nested sequence
  Messages::FieldCPtr decodedEntries;
  decodedPartyItem->getField("Entries", decodedEntries);
  Messages::SequenceCPtr decodedEntriesSequence = decodedEntries->toSequence();
  ASSERT_TRUE(decodedEntriesSequence);

  Messages::FieldSetCPtr decodedEntry = (*decodedEntriesSequence)[0];
  ASSERT_TRUE(decodedEntry);

  Messages::FieldCPtr decodedPrice;
  decodedEntry->getField("Price", decodedPrice);
  ASSERT_TRUE(decodedPrice);
  EXPECT_EQ((decodedPrice->toDecimal()), (Decimal(1,-1)));
}
