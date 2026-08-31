// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Dictionary Value layout: numeric entries must stay dense; public API and
// observable string/display behavior must match the pre-change semantics.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Common/Value.h>
#include <Common/StringBuffer.h>
#include <Common/Decimal.h>

#include <string>
#include <vector>

using namespace QuickFAST;

TEST(QuickFAST, testValueIsDenserThanEmbeddedStringBuffer)
{
  // Before Imp7: sizeof(Value)==128 with an embedded 96-byte StringBuffer.
  // After: string storage is on-demand; numeric entries pack much tighter
  // (measured 40 bytes on x86_64 RelWithDebInfo).
  EXPECT_LT(sizeof(Value), sizeof(StringBuffer));
  EXPECT_LE(sizeof(Value), 48u);
}

TEST(QuickFAST, testValueNumericRoundTripAndDisplay)
{
  Value v;
  v.setValue(int64(-42));
  EXPECT_TRUE(v.isSignedInteger());
  int64 got = 0;
  EXPECT_TRUE(v.getValue(got));
  EXPECT_EQ(-42, got);
  EXPECT_EQ("-42", std::string(reinterpret_cast<const char*>(v.displayString().c_str())));

  v.setValue(uint64(99));
  EXPECT_TRUE(v.isUnsignedInteger());
  EXPECT_EQ("99", std::string(reinterpret_cast<const char*>(v.displayString().c_str())));

  v.setValue(Decimal(12345, -2));
  EXPECT_TRUE(v.isNumeric());
  Decimal d;
  EXPECT_TRUE(v.getValue(d));
  EXPECT_EQ(12345, d.getMantissa());
  EXPECT_EQ(-2, d.getExponent());

  v.setNull();
  EXPECT_TRUE(v.isNull());
  EXPECT_EQ("[null]", std::string(reinterpret_cast<const char*>(v.displayString().c_str())));

  v.erase();
  EXPECT_FALSE(v.isDefined());
}

TEST(QuickFAST, testValueStringAndEquality)
{
  Value a;
  a.setValue("hello");
  Value b;
  b.setValue(reinterpret_cast<const unsigned char*>("hello"), 5);
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(a.isString());
  const unsigned char * p = 0;
  size_t n = 0;
  EXPECT_TRUE(a.getValue(p, n));
  EXPECT_EQ(5u, n);

  Value c;
  c.setValue(int64(1));
  Value d;
  d.setValue(int64(1));
  EXPECT_TRUE(c == d);
  c.setValue(int64(2));
  EXPECT_TRUE(c != d);

  Value undef;
  EXPECT_FALSE(undef == undef); // UNDEFINED never equals
}

TEST(QuickFAST, testValueLongStringAndCopyAssign)
{
  const std::string longStr(80, 'x');
  Value a;
  a.setValue(longStr);
  Value b(a);
  EXPECT_TRUE(a == b);
  std::string got;
  EXPECT_TRUE(b.getValue(got));
  EXPECT_EQ(longStr, got);

  Value c;
  c.setValue(int64(7));
  c = a;
  EXPECT_TRUE(c.isString());
  EXPECT_TRUE(c.getValue(got));
  EXPECT_EQ(longStr, got);

  c.setValue(int64(3));
  EXPECT_TRUE(c.isSignedInteger());
  EXPECT_FALSE(c.isString());
}

TEST(QuickFAST, testValueEraseKeepsStringCapacity)
{
  // Context::reset() erases every dictionary entry between messages. Freeing
  // the buffer there would cost a free/malloc pair per string entry per
  // message, so erase must clear the contents and keep the allocation.
  Value v;
  v.setValue(std::string(80, 'x'));
  const size_t grownCapacity = v.displayString().capacity();
  ASSERT_GT(grownCapacity, 48u) << "expected the long value to leave the inline buffer";

  v.erase();
  ASSERT_FALSE(v.isDefined());

  // A short value after the erase: a retained buffer still reports the grown
  // capacity, a freshly allocated one would be back to the inline size. Using
  // a long value here would not distinguish the two -- both grow the same way.
  v.setValue(std::string(5, 'y'));
  EXPECT_EQ(grownCapacity, v.displayString().capacity())
    << "erase must clear the buffer, not free it";
}

TEST(QuickFAST, testValueDictionaryStyleNumericArrayDensity)
{
  // Simulate an integer-heavy dictionary: Values must be assignable in place
  // without forcing a StringBuffer allocation for each slot.
  std::vector<Value> dict(1024);
  for(size_t i = 0; i < dict.size(); ++i)
  {
    dict[i].setValue(static_cast<uint64>(i));
  }
  uint64 probe = 0;
  EXPECT_TRUE(dict[100].getValue(probe));
  EXPECT_EQ(100u, probe);
  dict[100].erase();
  EXPECT_FALSE(dict[100].isDefined());
  dict[100].setValue("tail");
  EXPECT_TRUE(dict[100].isString());
  dict[100].setValue(uint64(1));
  EXPECT_TRUE(dict[100].isUnsignedInteger());
}
