// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <stdexcept>

#include <Communication/LinkedBuffer.h>

using namespace QuickFAST;

TEST(QuickFAST, testLinkedBufferIndexesAnExternalBuffer)
{
  // An external buffer is exactly one with capacity_ == 0, so the bounds check
  // "index >= capacity_" was unconditionally true for size_t and every index
  // threw, valid ones included.
  const unsigned char payload[] = {10, 20, 30, 40};
  Communication::LinkedBuffer buffer(payload, sizeof(payload));

  ASSERT_EQ((buffer.used()), (sizeof(payload)));
  EXPECT_EQ((buffer[0]), (10));
  EXPECT_EQ((buffer[3]), (40));

  const Communication::LinkedBuffer & constBuffer = buffer;
  EXPECT_EQ((constBuffer[0]), (10));
  EXPECT_EQ((constBuffer[3]), (40));

  // Out of range must still be rejected.
  EXPECT_THROW((void)buffer[4], std::range_error);
  EXPECT_THROW((void)constBuffer[4], std::range_error);
}

TEST(QuickFAST, testLinkedBufferIndexesAnOwnedBuffer)
{
  // An owned buffer bounds against capacity, which stays reachable even before
  // the whole allocation has been used.
  Communication::LinkedBuffer buffer(8);
  ASSERT_EQ((buffer.capacity()), (8u));
  buffer[0] = 99;
  buffer[7] = 77;
  EXPECT_EQ((buffer[0]), (99));
  EXPECT_EQ((buffer[7]), (77));
  EXPECT_THROW((void)buffer[8], std::range_error);
}
