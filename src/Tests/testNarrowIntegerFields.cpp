// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Narrow integer fields (8/16 bit) share the absence contract with the wider
// ones. Present values must round through their typed accessors, and
// toStringBuffer must still produce a readable form used by formatters.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/FieldInt8.h>
#include <Messages/FieldUInt8.h>
#include <Messages/FieldInt16.h>
#include <Messages/FieldUInt16.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

/// @brief Present narrow integers report their value and type correctly.
TEST(QuickFAST, testNarrowIntegerFieldsPresentValues)
{
  EXPECT_EQ(
    std::numeric_limits<int8>::min(),
    Messages::FieldInt8::create(std::numeric_limits<int8>::min())->toInt8());
  EXPECT_EQ(
    std::numeric_limits<int8>::max(),
    Messages::FieldInt8::create(std::numeric_limits<int8>::max())->toInt8());
  EXPECT_EQ(
    0,
    Messages::FieldInt8::create(0)->toInt8());

  EXPECT_EQ(
    uchar(0),
    Messages::FieldUInt8::create(0)->toUInt8());
  EXPECT_EQ(
    uchar(255),
    Messages::FieldUInt8::create(255)->toUInt8());

  EXPECT_EQ(
    std::numeric_limits<int16>::min(),
    Messages::FieldInt16::create(std::numeric_limits<int16>::min())->toInt16());
  EXPECT_EQ(
    std::numeric_limits<int16>::max(),
    Messages::FieldInt16::create(std::numeric_limits<int16>::max())->toInt16());

  EXPECT_EQ(
    uint16(0),
    Messages::FieldUInt16::create(0)->toUInt16());
  EXPECT_EQ(
    std::numeric_limits<uint16>::max(),
    Messages::FieldUInt16::create(std::numeric_limits<uint16>::max())->toUInt16());
}

/// @brief Narrow integers advertise as signed/unsigned and keep their type tags.
TEST(QuickFAST, testNarrowIntegerFieldsTypeTags)
{
  Messages::FieldCPtr i8 = Messages::FieldInt8::create(-7);
  EXPECT_TRUE(i8->isSignedInteger());
  EXPECT_TRUE(i8->isType(ValueType::INT8));
  EXPECT_EQ(-7, i8->toInt8());

  Messages::FieldCPtr u8 = Messages::FieldUInt8::create(255);
  EXPECT_FALSE(u8->isSignedInteger());
  EXPECT_TRUE(u8->isType(ValueType::UINT8));
  EXPECT_EQ(uchar(255), u8->toUInt8());

  Messages::FieldCPtr i16 = Messages::FieldInt16::create(42);
  EXPECT_TRUE(i16->isSignedInteger());
  EXPECT_TRUE(i16->isType(ValueType::INT16));
  EXPECT_EQ(42, i16->toInt16());

  Messages::FieldCPtr u16 = Messages::FieldUInt16::create(1000);
  EXPECT_FALSE(u16->isSignedInteger());
  EXPECT_TRUE(u16->isType(ValueType::UINT16));
  EXPECT_EQ(uint16(1000), u16->toUInt16());
}

/// @brief Wrong-type conversion stays UnsupportedConversion, not FieldNotPresent.
TEST(QuickFAST, testNarrowIntegerWrongTypeIsUnsupportedConversion)
{
  EXPECT_THROW(
    (void)Messages::FieldInt16::create(1)->toAscii(),
    UnsupportedConversion);
  EXPECT_THROW(
    (void)Messages::FieldUInt8::create(1)->toDecimal(),
    UnsupportedConversion);
}
