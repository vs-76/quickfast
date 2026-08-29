// Copyright (c) 2026, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include <gtest/gtest.h>

#include <Communication/RecoveryFeed.h>

#include <atomic>
#include <thread>

using namespace QuickFAST;

namespace
{
  /// @brief The smallest concrete feed the abstract base will accept.
  class TestFeed : public Communication::RecoveryFeed
  {
  public:
    bool reportGap(sequence_t, sequence_t) override
    {
      return true;
    }

    bool stillWaiting(sequence_t, sequence_t) override
    {
      return true;
    }
  };

  /// @brief A buffer the feed can hand out, owned by the caller.
  ///
  /// The storage is held separately rather than as a member: passing the
  /// address of a member array to a base constructor is legal but reads as
  /// use-before-initialisation to the compiler, and this is a test.
  class OwnedBuffer : public Communication::LinkedBuffer
  {
  public:
    OwnedBuffer()
      : Communication::LinkedBuffer(new unsigned char[16], 16)
    {
    }

    ~OwnedBuffer()
    {
      delete [] get();
    }
  };

  /// @brief Wait for a flag, but never longer than the test can afford.
  bool waitFor(const std::atomic<bool> & flag, std::chrono::milliseconds limit)
  {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while(!flag.load())
    {
      if(std::chrono::steady_clock::now() > deadline)
      {
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
  }
}

/// @brief A producer parked in waitFreeBuffer must be released at shutdown.
///
/// Before the fix there was no way to break this wait: no stop(), no flag in
/// the predicate. The only exits were to abandon the thread or kill the
/// process.
TEST(QuickFAST, testWaitFreeBufferIsReleasedByStop)
{
  TestFeed feed;
  std::atomic<bool> entered(false);
  std::atomic<bool> returned(false);
  Communication::LinkedBuffer * result = reinterpret_cast<Communication::LinkedBuffer *>(1);

  std::thread producer([&]
    {
      entered = true;
      result = feed.waitFreeBuffer();
      returned = true;
    });

  ASSERT_TRUE(waitFor(entered, std::chrono::milliseconds(1000)));
  // Give the thread time to reach the wait rather than merely the flag.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_FALSE(returned.load());

  feed.stop();
  EXPECT_TRUE(waitFor(returned, std::chrono::milliseconds(1000)));
  producer.join();
  EXPECT_TRUE(result == 0);
}

/// @brief After stop() the wait must not park again.
TEST(QuickFAST, testWaitFreeBufferAfterStopReturnsImmediately)
{
  TestFeed feed;
  feed.stop();
  EXPECT_TRUE(feed.waitFreeBuffer() == 0);
  EXPECT_TRUE(feed.stopping());
}

/// @brief A seeded pool still hands out its buffers.
TEST(QuickFAST, testWaitFreeBufferStillDeliversASeededPool)
{
  TestFeed feed;
  OwnedBuffer buffer;
  feed.releaseBuffer(&buffer);
  EXPECT_EQ(&buffer, feed.waitFreeBuffer());
  EXPECT_TRUE(feed.getFreeBuffer() == 0);
}

/// @brief A buffer released while a producer waits must reach it.
TEST(QuickFAST, testWaitFreeBufferWakesOnARelease)
{
  TestFeed feed;
  OwnedBuffer buffer;
  std::atomic<bool> returned(false);
  Communication::LinkedBuffer * result = 0;

  std::thread producer([&]
    {
      result = feed.waitFreeBuffer();
      returned = true;
    });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  feed.releaseBuffer(&buffer);
  EXPECT_TRUE(waitFor(returned, std::chrono::milliseconds(1000)));
  producer.join();
  EXPECT_EQ(&buffer, result);
}

/// @brief waitGapFill must say whether data arrived or the deadline expired.
///
/// The predicated wait_until already computes exactly this and the return
/// value was being dropped, leaving the assembler to re-poll at 100 Hz with no
/// way to tell a filling gap from a permanently unfillable one.
TEST(QuickFAST, testWaitGapFillReportsATimeout)
{
  TestFeed feed;
  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(feed.waitGapFill(std::chrono::milliseconds(20)));
  EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(20));
}

/// @brief waitGapFill reports success when a buffer is already waiting.
TEST(QuickFAST, testWaitGapFillReportsDataAlreadyPresent)
{
  TestFeed feed;
  OwnedBuffer buffer;
  feed.acceptBuffer(&buffer);
  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(feed.waitGapFill(std::chrono::milliseconds(5000)));
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(1000));
}

/// @brief waitGapFill reports success when a buffer arrives during the wait.
TEST(QuickFAST, testWaitGapFillReportsALateArrival)
{
  TestFeed feed;
  OwnedBuffer buffer;
  std::thread producer([&]
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      feed.acceptBuffer(&buffer);
    });

  EXPECT_TRUE(feed.waitGapFill(std::chrono::milliseconds(5000)));
  producer.join();

  Communication::BufferQueue queue;
  feed.fetchBuffers(queue);
  EXPECT_EQ(&buffer, queue.pop());
}

/// @brief stop() also releases a producer waiting for a gap fill.
TEST(QuickFAST, testWaitGapFillIsReleasedByStop)
{
  TestFeed feed;
  std::atomic<bool> returned(false);
  bool result = true;

  std::thread waiter([&]
    {
      result = feed.waitGapFill(std::chrono::hours(1));
      returned = true;
    });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  EXPECT_FALSE(returned.load());
  feed.stop();
  EXPECT_TRUE(waitFor(returned, std::chrono::milliseconds(1000)));
  waiter.join();
  EXPECT_FALSE(result);
}
