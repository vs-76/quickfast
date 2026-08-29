// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Template.h>
#include <Codecs/FieldInstruction.h>
#include <sstream>

using namespace QuickFAST;
TEST(QuickFAST, testXMLTemplateParser)
{
  SCOPED_TRACE("Start testXMLTemplateParser");
  Codecs::XMLTemplateParser parser;
  std::stringstream myDocument;

  myDocument << "<templates>" << std::endl;
  myDocument << "  <template name='Test1' id='203'>" << std::endl;
  myDocument << "    <typeRef name='MarketTest' />" << std::endl;
  myDocument << "    <int32 name='Int1' id='23'>" << std::endl;
  myDocument << "      <constant value='4' />" << std::endl;
  myDocument << "    </int32>" << std::endl;
  myDocument << "    <uInt64 name='UInt1' />" << std::endl;
  myDocument << "    <decimal name='Decimal1' presence='optional'>" << std::endl;
  myDocument << "      <exponent>" << std::endl;
  myDocument << "        <copy />" << std::endl;
  myDocument << "      </exponent>" << std::endl;
  myDocument << "      <mantissa>" << std::endl;
  myDocument << "        <delta />" << std::endl;
  myDocument << "      </mantissa>" << std::endl;
  myDocument << "    </decimal>" << std::endl;
  myDocument << "    <uInt8 name=\"Issue22\" id=\"Issue22\">" << std::endl;
  myDocument << "      <default value=\"20\"/>" << std::endl;
  myDocument << "    </uInt8>" << std::endl;
  myDocument << "    <byteVector name=\"byteVector\">" << std::endl;
  myDocument << "      <length name=\"byteVectorLength\"/>" << std::endl;
  myDocument << "    </byteVector>" << std::endl;

  myDocument << "  </template>" << std::endl;
  myDocument << "</templates>" << std::endl;

  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(myDocument);

  ASSERT_TRUE(templateRegistry);

  Codecs::TemplateCPtr template203;
  EXPECT_TRUE(!templateRegistry->getTemplate(205, template203));
  EXPECT_TRUE(templateRegistry->getTemplate(203, template203));
  EXPECT_EQ((template203->getId()), (template_id_t(203)));
  EXPECT_EQ((template203->getTemplateName()), ("Test1"));
  Codecs::FieldInstructionCPtr instruction;
  EXPECT_TRUE(!template203->getInstruction("Int2", instruction));
  EXPECT_TRUE(template203->getInstruction("Int1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t("23")));
  EXPECT_EQ((instruction->getName()), ("Int1"));
  Codecs::FieldOpCPtr fieldOp;
  fieldOp = instruction->getFieldOp();
  // ...

  EXPECT_TRUE(template203->getInstruction("UInt1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t()));
  EXPECT_EQ((instruction->getName()), ("UInt1"));
  fieldOp = instruction->getFieldOp();

  EXPECT_TRUE(template203->getInstruction("Decimal1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t()));
  EXPECT_EQ((instruction->getName()), ("Decimal1"));
  fieldOp = instruction->getFieldOp();
  Codecs::FieldInstructionCPtr exponent;
  EXPECT_TRUE(instruction->getMantissaInstruction(exponent));
  Codecs::FieldInstructionCPtr mantissa;
  EXPECT_TRUE(instruction->getMantissaInstruction(mantissa));
}

TEST(QuickFAST, testXMLTemplateParser2)
{
  Codecs::XMLTemplateParser parser;
  std::stringstream myDocument;

  // Test without <templates>
  myDocument << "  <template name='Test1' id='203'>" << std::endl;
  myDocument << "    <typeRef name='MarketTest' />" << std::endl;
  myDocument << "    <int32 name='Int1' id='23'>" << std::endl;
  myDocument << "      <constant value='4' />" << std::endl;
  myDocument << "    </int32>" << std::endl;
  myDocument << "    <uInt64 name='UInt1' />" << std::endl;
  myDocument << "    <decimal name='Decimal1' presence='optional'>" << std::endl;
  myDocument << "      <exponent>" << std::endl;
  myDocument << "        <copy />" << std::endl;
  myDocument << "      </exponent>" << std::endl;
  myDocument << "      <mantissa>" << std::endl;
  myDocument << "        <delta />" << std::endl;
  myDocument << "      </mantissa>" << std::endl;
  myDocument << "    </decimal>" << std::endl;
  myDocument << "  </template>" << std::endl;

  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(myDocument);

  ASSERT_TRUE(templateRegistry);

  Codecs::TemplateCPtr template203;
  EXPECT_TRUE(!templateRegistry->getTemplate(205, template203));
  ASSERT_TRUE(templateRegistry->getTemplate(203, template203));
  EXPECT_EQ((template203->getId()), (template_id_t(203)));
  EXPECT_EQ((template203->getTemplateName()), ("Test1"));
  Codecs::FieldInstructionCPtr instruction;
  EXPECT_TRUE(!template203->getInstruction("Int2", instruction));
  EXPECT_TRUE(template203->getInstruction("Int1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t("23")));
  EXPECT_EQ((instruction->getName()), ("Int1"));
  Codecs::FieldOpCPtr fieldOp;
  fieldOp = instruction->getFieldOp();
  // ...

  ASSERT_TRUE(template203->getInstruction("UInt1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t()));
  EXPECT_EQ((instruction->getName()), ("UInt1"));
  fieldOp = instruction->getFieldOp();

  ASSERT_TRUE(template203->getInstruction("Decimal1", instruction));
  EXPECT_EQ((instruction->getId()), (field_id_t()));
  EXPECT_EQ((instruction->getName()), ("Decimal1"));
  fieldOp = instruction->getFieldOp();
  Codecs::FieldInstructionCPtr exponent;
  EXPECT_TRUE(instruction->getMantissaInstruction(exponent));
  Codecs::FieldInstructionCPtr mantissa;
  EXPECT_TRUE(instruction->getMantissaInstruction(mantissa));
}


TEST(QuickFAST, testXMLTemplateParser3)
{
  Codecs::XMLTemplateParser parser;
  std::stringstream myDocument;

  myDocument << "<template name=\"MarketUpdate\" id=\"1\">" << std::endl;
  myDocument << "  <uInt32 name=\"MsgType\" id=\"0\" presence=\"mandatory\"> <constant value=\"702\"/> </uInt32>" << std::endl;
  myDocument << "  <uInt32 name=\"SymbolIndex\" id=\"1\" presence=\"mandatory\"> <default value=\"0\"/> </uInt32>" << std::endl;
  myDocument << "  <uInt32 name=\"SecurityIDSource\" id=\"2\" presence=\"mandatory\"> <default value=\"0\"/> </uInt32>" << std::endl;
  myDocument << "  <string name=\"SecurityID\" id=\"3\" presence=\"mandatory\"> <default value=\"               \"/> </string>" << std::endl;
  myDocument << "  <uInt32 name=\"SourceTime\" id=\"4\" presence=\"mandatory\"> <delta/> </uInt32>" << std::endl;
  myDocument << "  <uInt32 name=\"SeriesSequenceNumber\" id=\"5\" presence=\"mandatory\"> </uInt32>" << std::endl;
  myDocument << "  <uInt32 name=\"SnapshotFlag\" id=\"6\" presence=\"mandatory\"> <default value=\"0\"/> </uInt32>" << std::endl;
  myDocument << "  <sequence name=\"UpdateSubMsg\">" << std::endl;
  myDocument << "    <length name=\"UpdateCount\" id=\"7\"> <default value=\"1\"/> </length>" << std::endl;
  myDocument << "    <uInt32 name=\"UpdateType\" id=\"8\" presence=\"mandatory\"> </uInt32>" << std::endl;
  myDocument << "    <int32 name=\"Price\" id=\"9\" presence=\"optional\"> <delta/> </int32>" << std::endl;
  myDocument << "    <uInt32 name=\"Volume\" id=\"10\" presence=\"mandatory\"> </uInt32>" << std::endl;
  myDocument << "  </sequence>" << std::endl;
  myDocument << "</template>" << std::endl;

  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(myDocument);

  ASSERT_TRUE(templateRegistry);

  Codecs::TemplateCPtr template1;
  ASSERT_TRUE(templateRegistry->getTemplate(1, template1));
  ASSERT_EQ((template1->size()), (8));
  size_t fieldIndex = 0;
  Codecs::FieldOpCPtr fieldOp;
  Codecs::FieldInstructionCPtr instruction;
  //MsgType
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));

  //SymbolIndex
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));

  //SecurityIDSource
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));

  //SecurityID
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));

  //SourceTime
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));

  //SeriesSequenceNumber
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));

  //SnapshotFlag
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));

  //UpdateSubMsg
  instruction = template1->getInstruction(fieldIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));

  Codecs::SegmentBodyPtr segment;
  ASSERT_TRUE(instruction->getSegmentBody(segment));

  // the pmap bit used by the Length field should be counted in the
  // enclosing segment.  The instructions in this segment use no
  // presence map bits.
  EXPECT_EQ((segment->presenceMapBitCount()), (0));

  size_t segmentIndex = 0;
  //UpdateType
  instruction = segment->getInstruction(segmentIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));

  //Price
  instruction = segment->getInstruction(segmentIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(false));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));

  //Volume
  instruction = segment->getInstruction(segmentIndex++);
  fieldOp = instruction->getFieldOp();
  EXPECT_TRUE(!fieldOp->usesPresenceMap(true));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));
}

const char GroupSequencePMAPXML[] =
"\
<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<templates xmlns=\"http://www.fixprotocol.org/ns/fast/td/1.1\">\n\
  <template name=\"GroupPMAPTest\" id=\"9998\">\n\
    <group name=\"noMapNoBit\">\n\
      <int32 name=\"one\"/>\n\
    </group>\n\
    <group name=\"BitNoMap\" presence=\"optional\">\n\
      <int32 name=\"two\"/>\n\
    </group>\n\
    <group name=\"MapNoBit\">\n\
      <int32 name=\"three\">\n\
        <default value=\"1\"/>\n\
      </int32>\n\
    </group>\n\
    <group name=\"BitAndMap\" presence=\"optional\">\n\
      <int32 name=\"four\">\n\
        <default value=\"1\"/>\n\
      </int32>\n\
    </group>\n\
  </template>\n\
  <template name=\"SequencePMAPTest\" id=\"9997\">\n\
    <sequence name=\"noMapNoBit\">\n\
      <length name=\"NoNoMapNoBit\"/>\n\
      <int32 name=\"one\"/>\n\
    </sequence>\n\
    <sequence name=\"BitNoMap\" presence=\"optional\">\n\
      <length name=\"NoNoMapNoBit\"><copy value=\"0\"/></length>\n\
      <int32 name=\"two\"/>\n\
    </sequence>\n\
    <sequence name=\"MapNoBit\">\n\
      <length name=\"NoNoMapNoBit\"/>\n\
      <int32 name=\"three\">\n\
        <default value=\"1\"/>\n\
      </int32>\n\
    </sequence>\n\
    <sequence name=\"BitAndMap\" presence=\"optional\">\n\
      <length name=\"NoNoMapNoBit\"><copy value=\"0\"/></length>\n\
      <int32 name=\"four\">\n\
        <default value=\"1\"/>\n\
      </int32>\n\
    </sequence>\n\
    <sequence name=\"anotherBitNoMap\">\n\
      <length name=\"NoAnotherBitNoMap\"><copy value=\"0\"/></length>\n\
      <int32 name=\"five\"/>\n\
    </sequence>\n\
  </template>\n\
</templates>\n\
";

TEST(QuickFAST, testGroupsAndSequencesVsPMAP)
{
  SCOPED_TRACE("Start testXMLTemplateParser");
  Codecs::XMLTemplateParser parser;
  std::stringstream xmlin(GroupSequencePMAPXML);

  Codecs::TemplateRegistryPtr templateRegistry =
    parser.parse(xmlin);

  ASSERT_TRUE(templateRegistry);

  /////////
  // Groups
  Codecs::TemplateCPtr template9998;
  ASSERT_TRUE(templateRegistry->getTemplate(9998, template9998));
  EXPECT_EQ((template9998->presenceMapBitCount()), (3));

  // noMapNoBit
  Codecs::FieldInstructionCPtr instruction;
  ASSERT_TRUE(template9998->getInstruction(0, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));
  Codecs::SegmentBodyPtr segment;
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (0));

  // BitNoMap
  ASSERT_TRUE(template9998->getInstruction(1, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (0));

  // MapNoBit
  ASSERT_TRUE(template9998->getInstruction(2, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (1));

  // BitAndMap
  ASSERT_TRUE(template9998->getInstruction(3, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (1));

  ////////////
  // Sequences

  Codecs::TemplateCPtr template9997;
  ASSERT_TRUE(templateRegistry->getTemplate(9997, template9997));
  EXPECT_EQ((template9997->presenceMapBitCount()), (4));

  // noMapNoBit
  ASSERT_TRUE(template9997->getInstruction(0, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (0));

  // BitNoMap
  ASSERT_TRUE(template9997->getInstruction(1, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (0));

  // MapNoBit
  ASSERT_TRUE(template9997->getInstruction(2, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (0));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (1));

  // BitAndMap
  ASSERT_TRUE(template9997->getInstruction(3, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (1));

  // anotherBitNoMap
  ASSERT_TRUE(template9997->getInstruction(4, instruction));
  EXPECT_EQ((instruction->getPresenceMapBitsUsed()), (1));
  ASSERT_TRUE(instruction->getSegmentBody(segment));
  EXPECT_EQ((segment->presenceMapBitCount()), (0));
}
