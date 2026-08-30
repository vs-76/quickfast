// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// WorkingBuffer::push(bytes, count) must match a sequence of push(byte) in both
// forward and reverse modes, including growth and zero-length.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Common/WorkingBuffer.h>
#include <string>
#include <vector>

using namespace QuickFAST;

namespace
{
  void pushOneByOne(WorkingBuffer & buffer, const uchar * bytes, size_t count)
  {
    for(size_t i = 0; i < count; ++i)
    {
      buffer.push(bytes[i]);
    }
  }

  std::vector<uchar> contents(const WorkingBuffer & buffer)
  {
    return std::vector<uchar>(buffer.begin(), buffer.end());
  }
}

TEST(QuickFAST, testWorkingBufferBulkPushForwardMatchesBytePush)
{
  const uchar bytes[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  const size_t n = sizeof(bytes);

  WorkingBuffer expected;
  expected.clear(false);
  pushOneByOne(expected, bytes, n);

  WorkingBuffer actual;
  actual.clear(false);
  actual.push(bytes, n);

  EXPECT_EQ(contents(expected), contents(actual));
  EXPECT_TRUE(expected == actual);
}

TEST(QuickFAST, testWorkingBufferBulkPushReverseMatchesBytePush)
{
  const uchar bytes[] = {'a', 'b', 'c', 'd', 'e'};
  const size_t n = sizeof(bytes);

  WorkingBuffer expected;
  expected.clear(true);
  pushOneByOne(expected, bytes, n);

  WorkingBuffer actual;
  actual.clear(true);
  actual.push(bytes, n);

  EXPECT_EQ(contents(expected), contents(actual));
  EXPECT_TRUE(expected == actual);
}

TEST(QuickFAST, testWorkingBufferBulkPushZeroAndGrowAndAppendToExisting)
{
  WorkingBuffer buffer;
  buffer.clear(false, 4);
  const uchar first[] = {10, 20};
  buffer.push(first, 0); // zero-length no-op
  EXPECT_EQ(0u, buffer.size());

  buffer.push(first, 2);
  EXPECT_EQ(2u, buffer.size());

  // Force growth past initial small capacity.
  std::vector<uchar> big(64);
  for(size_t i = 0; i < big.size(); ++i)
  {
    big[i] = static_cast<uchar>(i);
  }
  WorkingBuffer expected;
  expected.clear(false, 4);
  pushOneByOne(expected, first, 2);
  pushOneByOne(expected, big.data(), big.size());

  buffer.push(big.data(), big.size());
  EXPECT_EQ(contents(expected), contents(buffer));
}

TEST(QuickFAST, testWorkingBufferBulkPushReverseGrows)
{
  const std::string payload(40, 'x');
  WorkingBuffer expected;
  expected.clear(true, 8);
  pushOneByOne(
    expected,
    reinterpret_cast<const uchar *>(payload.data()),
    payload.size());

  WorkingBuffer actual;
  actual.clear(true, 8);
  actual.push(reinterpret_cast<const uchar *>(payload.data()), payload.size());
  EXPECT_TRUE(expected == actual);
}
