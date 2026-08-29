// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Absence must be distinguishable from zero.
//
// FAST draws a hard line between a field that carries the value zero and a
// field that is not there, and every typed accessor was written to enforce it.
// Each one built a FieldNotPresent and then dropped it on the floor, so the
// line disappeared at the API boundary: an absent field read as 0, an absent
// decimal as 0.0, an absent byte vector as empty.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Messages/FieldInt8.h>
#include <Messages/FieldUInt8.h>
#include <Messages/FieldInt16.h>
#include <Messages/FieldUInt16.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldInt64.h>
#include <Messages/FieldUInt64.h>
#include <Messages/FieldDecimal.h>
#include <Messages/FieldByteVector.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldUtf8.h>

#include <Common/Exceptions.h>

using namespace QuickFAST;

/// @brief Every numeric accessor must refuse to invent a value for an absent field.
TEST(QuickFAST, testAbsentIntegerFieldsThrowFieldNotPresent)
{
  EXPECT_THROW((void)Messages::FieldInt8::createNull()->toInt8(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldUInt8::createNull()->toUInt8(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldInt16::createNull()->toInt16(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldUInt16::createNull()->toUInt16(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldInt32::createNull()->toInt32(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldUInt32::createNull()->toUInt32(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldInt64::createNull()->toInt64(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldUInt64::createNull()->toUInt64(),
    FieldNotPresent);
}

/// @brief The decimal and byte vector accessors are written the same way.
TEST(QuickFAST, testAbsentDecimalAndByteVectorThrowFieldNotPresent)
{
  EXPECT_THROW((void)Messages::FieldDecimal::createNull()->toDecimal(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldByteVector::createNull()->toByteVector(),
    FieldNotPresent);
}

/// @brief Absence and zero must not be confusable.
///
/// This is the reason the distinction matters: without the throw both of these
/// hand back the same number, and a receiver reads "not quoted" as "quoted at
/// zero".
TEST(QuickFAST, testAbsentIsDistinguishableFromZero)
{
  Messages::FieldCPtr zero = Messages::FieldUInt32::create(0);
  Messages::FieldCPtr absent = Messages::FieldUInt32::createNull();

  EXPECT_EQ(0u, zero->toUInt32());
  EXPECT_THROW((void)absent->toUInt32(), FieldNotPresent);
}

/// @brief A present field must still be readable, including a zero one.
TEST(QuickFAST, testPresentFieldsAreUnaffected)
{
  EXPECT_EQ(0, Messages::FieldInt8::create(0)->toInt8());
  EXPECT_EQ(-42, Messages::FieldInt32::create(-42)->toInt32());
  EXPECT_EQ(7u, Messages::FieldUInt64::create(7)->toUInt64());
  EXPECT_EQ(Decimal(123, -2),
    Messages::FieldDecimal::create(Decimal(123, -2))->toDecimal());
  EXPECT_NO_THROW(
    (void)Messages::FieldByteVector::create("abc")->toByteVector());
}

/// @brief String fields report absence the same way as everything else.
///
/// They reach it by a different route -- the check lives in the shared
/// Field::toString rather than in the derived accessor -- so a fix applied only
/// to the ten derived classes would leave these two throwing a different
/// exception for the same condition.
TEST(QuickFAST, testAbsentStringFieldsThrowFieldNotPresent)
{
  EXPECT_THROW((void)Messages::FieldAscii::createNull()->toAscii(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldUtf8::createNull()->toUtf8(),
    FieldNotPresent);
  EXPECT_THROW((void)Messages::FieldAscii::createNull()->toString(),
    FieldNotPresent);
}

/// @brief Asking a field for a type it does not hold is a different complaint.
///
/// Absence is FieldNotPresent; a wrong-type request stays UnsupportedConversion.
/// Collapsing the two would make it impossible to tell "the field is not here"
/// from "you asked the wrong question".
TEST(QuickFAST, testWrongTypeStillThrowsUnsupportedConversion)
{
  EXPECT_THROW((void)Messages::FieldUInt32::create(1)->toString(),
    UnsupportedConversion);
  EXPECT_THROW((void)Messages::FieldAscii::create("x")->toDecimal(),
    UnsupportedConversion);
}
