// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Common/StringBuffer.h>

#include <stdexcept>
#include <string>

using namespace QuickFAST;

TEST(QuickFAST, testStringBufferResizeWorksOnAWritableBuffer)
{
  // The delegate check was inverted. A delegated buffer is read-only, so the
  // guarded body could only ever throw, while the writable case -- the one
  // resize exists for -- fell through and did nothing at all.
  StringBuffer buffer(reinterpret_cast<const unsigned char *>("abc"), 3);
  ASSERT_EQ((buffer.size()), (3u));

  buffer.resize(6, 'x');
  EXPECT_EQ((buffer.size()), (6u));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size())),
    (std::string("abcxxx")));

  buffer.resize(2);
  EXPECT_EQ((buffer.size()), (2u));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size())),
    (std::string("ab")));

  // Same length: nothing changes, and nothing throws.
  buffer.resize(2);
  EXPECT_EQ((buffer.size()), (2u));
}

TEST(QuickFAST, testStringBufferResizeRefusesADelegatedBuffer)
{
  // A delegated buffer wraps a const std::string and cannot be written, so
  // resize must refuse rather than silently corrupt or silently succeed.
  const std::string owner("delegated");
  StringBuffer buffer(&owner);
  ASSERT_EQ((buffer.size()), (owner.size()));

  EXPECT_THROW(buffer.resize(20, 'x'), std::logic_error);
  EXPECT_THROW(buffer.resize(2), std::logic_error);
  EXPECT_EQ((buffer.size()), (owner.size()));
}

TEST(QuickFAST, testStringBufferAssignsASingleCharacter)
{
  // temp(rhs, 1) cannot match StringBufferT(const unsigned char *, size_t),
  // so overload resolution picked StringBufferT(size_t length, unsigned char)
  // and built a run of `rhs` bytes of 0x01: assigning 'A' produced 65 bytes.
  StringBuffer buffer;
  buffer = static_cast<unsigned char>('A');

  EXPECT_EQ((buffer.size()), (1u));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer.data()), buffer.size())),
    (std::string("A")));

  // Zero is a legal character and must produce a one-byte string, not empty.
  buffer = static_cast<unsigned char>(0);
  EXPECT_EQ((buffer.size()), (1u));
  EXPECT_EQ((buffer[0]), (0));
}

namespace
{
  size_t growthAppending(StringBuffer & buffer, size_t bytes)
  {
    const unsigned char byte = 'z';
    for(size_t n = 0; n < bytes; ++n)
    {
      buffer.append(&byte, 1);
    }
    return buffer.growCount();
  }
}

TEST(QuickFAST, testStringBufferReusesInlineStorageAfterErase)
{
  // The delegating constructor reports zero capacity because the bytes belong
  // to someone else. erase() drops the delegate but left capacity_ at zero, so
  // the object claimed to have no room while owning INTERNAL_CAPACITY bytes of
  // inline storage: the very first append went to the heap.
  const std::string owner("delegated");
  StringBuffer erased(&owner);
  erased.erase();
  ASSERT_EQ((erased.size()), (0u));

  StringBuffer fresh;
  ASSERT_EQ((growthAppending(fresh, 10)), (0u));

  // Ten bytes fit inline, so neither buffer has any reason to allocate.
  EXPECT_EQ((growthAppending(erased, 10)), (0u));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(erased.data()), erased.size())),
    (std::string(10, 'z')));
}

TEST(QuickFAST, testStringBufferGrowsGeometricallyAfterErase)
{
  // Doubling does re-engage once capacity is non-zero, so the cost here is
  // bounded rather than quadratic -- but it should match a buffer that was
  // never delegated, not merely stay in the same order of magnitude.
  const std::string owner("delegated");
  StringBuffer erased(&owner);
  erased.erase();
  StringBuffer fresh;

  EXPECT_EQ((growthAppending(erased, 4096)), (growthAppending(fresh, 4096)));
}
