// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Messages/FieldUInt32.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldAscii.h>

#include <string>
#include <thread>
#include <vector>

using namespace QuickFAST;
using namespace QuickFAST::Messages;

TEST(QuickFAST, testFieldEqualityDistinguishesAbsentFromZero)
{
  // "Absent" and "zero" are different answers in FAST, and the value members
  // are all default initialized, so an undefined field carries the same zero
  // an ordinary zero-valued field does. Comparing only the values reported
  // them equal.
  FieldCPtr absent = FieldUInt32::createNull();
  FieldCPtr zero = FieldUInt32::create(0);

  ASSERT_FALSE(absent->isDefined());
  ASSERT_TRUE(zero->isDefined());

  EXPECT_FALSE((*absent == *zero));
  EXPECT_TRUE((*absent != *zero));

  // Two absent fields of the same type are still equal to each other.
  FieldCPtr alsoAbsent = FieldUInt32::createNull();
  EXPECT_TRUE((*absent == *alsoAbsent));

  // And equal values still compare equal.
  EXPECT_TRUE((*zero == *FieldUInt32::create(0)));
  EXPECT_FALSE((*zero == *FieldUInt32::create(1)));
}

TEST(QuickFAST, testFieldEqualityDistinguishesAbsentFromEmptyString)
{
  FieldCPtr absent = FieldAscii::createNull();
  FieldCPtr empty = FieldAscii::create("");

  ASSERT_FALSE(absent->isDefined());
  ASSERT_TRUE(empty->isDefined());
  EXPECT_FALSE((*absent == *empty));
  EXPECT_TRUE((*empty == *FieldAscii::create("")));
}

TEST(QuickFAST, testFieldEqualityStillSeparatesTypes)
{
  EXPECT_FALSE((*FieldUInt32::create(1) == *FieldInt32::create(1)));
}

TEST(QuickFAST, testFieldDisplayStringIsSafeToShareAcrossThreads)
{
  // Fields are handed around as FieldCPtr -- shared_ptr<const Field> -- which
  // conventionally means safe to share. displayString() fills a mutable cache
  // from a const method, so two threads formatting the same decoded field
  // raced on a StringBuffer's pointer, size and capacity. Run under
  // -fsanitize=thread to see the difference.
  FieldCPtr field = FieldUInt32::create(1234567u);

  const size_t threadCount = 8;
  std::vector<std::thread> threads;
  std::vector<std::string> results(threadCount);
  for(size_t n = 0; n < threadCount; ++n)
  {
    threads.push_back(std::thread([&field, &results, n]{
      const StringBuffer & rendered = field->displayString();
      results[n] = std::string(
        reinterpret_cast<const char *>(rendered.data()), rendered.size());
    }));
  }
  for(std::thread & thread : threads)
  {
    thread.join();
  }

  for(size_t n = 0; n < threadCount; ++n)
  {
    EXPECT_EQ((results[n]), (std::string("1234567"))) << "thread " << n;
  }
}
