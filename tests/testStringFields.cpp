// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldUtf8.h>
#include <Messages/FieldByteVector.h>
#include <Common/Exceptions.h>

using namespace ::QuickFAST;

TEST(QuickFAST, testFieldAscii)
{
  Messages::FieldCPtr field = Messages::FieldAscii::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldAscii::create("Hello, World!");
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((field->toAscii()), ("Hello, World!"));

  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
  EXPECT_THROW(field->toUtf8(), UnsupportedConversion);
}


TEST(QuickFAST, testFieldUtf8)
{
  Messages::FieldCPtr field = Messages::FieldUtf8::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldUtf8::create("Hello, World!");
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((field->toUtf8()), ("Hello, World!"));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
}

TEST(QuickFAST, testFieldByteVector)
{
  Messages::FieldCPtr field = Messages::FieldByteVector::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldByteVector::create("Hello, World!");
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((field->toByteVector()), ("Hello, World!"));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
}


