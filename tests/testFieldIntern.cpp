// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/Field.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldInt8.h>
#include <Messages/FieldAscii.h>

#include <string>
#include <thread>
#include <vector>

using namespace QuickFAST;
using namespace QuickFAST::Messages;

TEST(QuickFAST, testFieldInternSharesSmallIntegerIdentity)
{
  FieldCPtr a = FieldInt32::create(7);
  FieldCPtr b = FieldInt32::create(7);
  EXPECT_EQ(a.get(), b.get());
  EXPECT_TRUE(*a == *b);
  EXPECT_EQ(7, a->toInt32());

  FieldCPtr c = FieldUInt32::create(0);
  FieldCPtr d = FieldUInt32::create(0);
  EXPECT_EQ(c.get(), d.get());

  FieldCPtr outside = FieldInt32::create(100000);
  FieldCPtr outside2 = FieldInt32::create(100000);
  EXPECT_NE(outside.get(), outside2.get());
  EXPECT_TRUE(*outside == *outside2);
}

TEST(QuickFAST, testFieldInternNullAndEmptySingletons)
{
  EXPECT_EQ(FieldInt32::createNull().get(), FieldInt32::createNull().get());
  EXPECT_EQ(FieldAscii::createNull().get(), FieldAscii::createNull().get());
  EXPECT_EQ(FieldAscii::create("").get(), FieldAscii::create("").get());
  EXPECT_NE(FieldAscii::createNull().get(), FieldAscii::create("").get());
  EXPECT_FALSE(*FieldAscii::createNull() == *FieldAscii::create(""));
}

TEST(QuickFAST, testFieldInternInt8CoversFullRange)
{
  FieldCPtr lo = FieldInt8::create(static_cast<int8>(-128));
  FieldCPtr lo2 = FieldInt8::create(static_cast<int8>(-128));
  FieldCPtr hi = FieldInt8::create(static_cast<int8>(127));
  EXPECT_EQ(lo.get(), lo2.get());
  EXPECT_EQ(-128, lo->toInt8());
  EXPECT_EQ(127, hi->toInt8());
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
TEST(QuickFAST, testFieldInternReducesHeapCreatesForSmallInts)
{
  Field::resetHeapCreateCount();
  for(int32 v = -128; v <= 255; ++v)
  {
    (void)FieldInt32::create(v);
  }
  for(uint32 v = 0; v <= 255u; ++v)
  {
    (void)FieldUInt32::create(v);
  }
  EXPECT_EQ(0u, Field::heapCreateCount());

  Field::resetHeapCreateCount();
  (void)FieldInt32::create(100000);
  (void)FieldUInt32::create(100000u);
  EXPECT_EQ(2u, Field::heapCreateCount());
}
#endif

TEST(QuickFAST, testFieldInternDisplayStringSafeAcrossThreads)
{
  FieldCPtr field = FieldInt32::create(42); // interned identity
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
    EXPECT_EQ(std::string("42"), results[n]) << "thread " << n;
  }
}
