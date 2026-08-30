// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldInt64.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldUInt64.h>
#include <Common/Exceptions.h>

using namespace ::QuickFAST;

TEST(QuickFAST, testFieldInt32)
{
  Messages::FieldCPtr field = Messages::FieldInt32::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldInt32::create(23);
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((23), (field->toInt32()));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
  EXPECT_THROW(field->toUtf8(), UnsupportedConversion);
}


TEST(QuickFAST, testFieldInt64)
{
  Messages::FieldCPtr field = Messages::FieldInt64::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldInt64::create(4294967295L);
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  int64 value = field->toInt64();
  EXPECT_TRUE(value > 0);

  EXPECT_EQ((4294967295L), (value));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
  EXPECT_THROW(field->toUtf8(), UnsupportedConversion);

  int64 posValue = 0x7FFFFFFFFFFFFFFFLL;
  int64 negValue = 0x8000000000000000LL;
  field = Messages::FieldInt64::create(posValue);
  EXPECT_TRUE(field->isDefined());
  EXPECT_EQ((posValue), (field->toInt64()));
  EXPECT_TRUE(field->toInt64() > 0);

  field = Messages::FieldInt64::create(negValue);
  EXPECT_EQ((negValue), (field->toInt64()));
  EXPECT_TRUE(field->toInt64() < 0);

}

TEST(QuickFAST, testFieldUInt32)
{
  Messages::FieldCPtr field = Messages::FieldUInt32::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldUInt32::create(23);
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(field->isType(ValueType::UINT32));
  EXPECT_TRUE(!field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((23), (field->toUInt32()));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
  EXPECT_THROW(field->toUtf8(), UnsupportedConversion);

  field = Messages::FieldUInt32::create((uint32)-999999);
  EXPECT_TRUE(field->isDefined());
  EXPECT_EQ(((uint32)-999999), (field->toUInt32()));
  EXPECT_TRUE(field->toUInt32() > 0);
}

TEST(QuickFAST, testFieldUInt64)
{
  Messages::FieldCPtr field = Messages::FieldUInt64::createNull();
  EXPECT_TRUE(!field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  field = Messages::FieldUInt64::create(4294967295L);
  EXPECT_TRUE(field->isDefined());
  EXPECT_TRUE(!field->isType(ValueType::ASCII));
  EXPECT_TRUE(!field->isType(ValueType::BYTEVECTOR));
  EXPECT_TRUE(!field->isType(ValueType::DECIMAL));
  EXPECT_TRUE(!field->isType(ValueType::INT32));
  EXPECT_TRUE(!field->isType(ValueType::INT64));
  EXPECT_TRUE(!field->isType(ValueType::UINT32));
  EXPECT_TRUE(field->isType(ValueType::UINT64));
  EXPECT_TRUE(!field->isType(ValueType::UTF8));
  EXPECT_TRUE(!field->isType(ValueType::SEQUENCE));

  EXPECT_EQ((4294967295L), (field->toUInt64()));

  EXPECT_THROW(field->toAscii(), UnsupportedConversion);
  EXPECT_THROW(field->toInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toUInt32(), UnsupportedConversion);
  EXPECT_THROW(field->toInt64(), UnsupportedConversion);
  EXPECT_THROW(field->toDecimal(), UnsupportedConversion);
  EXPECT_THROW(field->toUtf8(), UnsupportedConversion);

  uint64 posValue = 0x7FFFFFFFFFFFFFFFLL;
  uint64 negValue = (uint64)0x8000000000000000LL;
  field = Messages::FieldUInt64::create(posValue);
  EXPECT_TRUE(field->isDefined());
  EXPECT_EQ((posValue), (field->toUInt64()));
  EXPECT_TRUE(field->toUInt64() > 0);

  field = Messages::FieldUInt64::create(negValue);
  EXPECT_EQ((negValue), (field->toUInt64()));
  EXPECT_TRUE(field->toUInt64() > 0);

}
