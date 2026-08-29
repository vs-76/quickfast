// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// DataDestination has two notions of how much it holds. The iterator pair and
// size() use used_, the count of buffers handed out this cycle; toString and
// toWorkingBuffer walk buffers_.size(), the whole pool including slots left
// over from an earlier, longer cycle. So the gather-write path and the string
// path can disagree about what the object contains.
//
// Reaching that disagreement needs a write past used_, and the only way to get
// one is selectBuffer, which takes a bare size_t and validates nothing. A
// handle from before a clear() is indistinguishable from a live one, so a
// stale handle steers putByte into a slot the destination does not consider
// part of the current message -- and toString then sends it.
//
// Both halves are fixed here: the conversions count what is used, and a handle
// that is not live is refused.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/DataDestination.h>
#include <Common/WorkingBuffer.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  /// @brief Start a fresh buffer holding one byte.
  Codecs::DataDestination::BufferHandle putOne(
    Codecs::DataDestination & destination,
    uchar byte)
  {
    const Codecs::DataDestination::BufferHandle handle = destination.startBuffer();
    destination.putByte(byte);
    return handle;
  }

  /// @brief What the gather-write path would send.
  std::string gathered(const Codecs::DataDestination & destination)
  {
    std::string result;
    Codecs::DataDestination::const_iterator it = destination.begin();
    Codecs::DataDestination::const_iterator stop = destination.end();
    while(it != stop)
    {
      const asio::const_buffer buffer = *it;
      result.append(
        static_cast<const char *>(buffer.data()), buffer.size());
      ++it;
    }
    return result;
  }

  /// @brief What toString would send.
  std::string stringified(const Codecs::DataDestination & destination)
  {
    std::string result;
    destination.toString(result);
    return result;
  }
}

/// @brief A stale handle must be refused, not silently written through.
TEST(QuickFAST, testStaleBufferHandleIsRejected)
{
  Codecs::DataDestination destination;
  putOne(destination, 'A');
  putOne(destination, 'B');
  const Codecs::DataDestination::BufferHandle third = putOne(destination, 'C');

  destination.clear();
  putOne(destination, 'Z');

  EXPECT_THROW(destination.selectBuffer(third), UsageError);
}

/// @brief An index past the end must be refused too.
TEST(QuickFAST, testBufferIndexIsRangeChecked)
{
  Codecs::DataDestination destination;
  putOne(destination, 'A');

  EXPECT_EQ(1u, destination.size());
  EXPECT_NO_THROW((void)destination[0]);
  EXPECT_THROW((void)destination[1], UsageError);
  EXPECT_THROW(destination.selectBuffer(1), UsageError);
}

/// @brief Every view of the contents must report the same bytes.
TEST(QuickFAST, testDataDestinationViewsAgree)
{
  Codecs::DataDestination destination;
  putOne(destination, 'A');
  putOne(destination, 'B');
  putOne(destination, 'C');

  EXPECT_EQ(3u, destination.size());
  EXPECT_EQ("ABC", stringified(destination));
  EXPECT_EQ("ABC", gathered(destination));

  WorkingBuffer working;
  destination.toWorkingBuffer(working);
  EXPECT_EQ("ABC",
    std::string(reinterpret_cast<const char *>(working.begin()), working.size()));
}

/// @brief A shorter cycle after a longer one reports only the shorter one.
TEST(QuickFAST, testDataDestinationDoesNotReportLeftoverBuffers)
{
  Codecs::DataDestination destination;
  putOne(destination, 'A');
  putOne(destination, 'B');
  putOne(destination, 'C');

  // The pool now holds three buffers. A shorter second cycle uses one of them
  // and leaves two behind; only the one belongs to this message.
  destination.clear();
  putOne(destination, 'Z');

  EXPECT_EQ(1u, destination.size());
  EXPECT_EQ("Z", stringified(destination));
  EXPECT_EQ("Z", gathered(destination));

  WorkingBuffer working;
  destination.toWorkingBuffer(working);
  EXPECT_EQ("Z",
    std::string(reinterpret_cast<const char *>(working.begin()), working.size()));
}

/// @brief Reselecting a live handle still works, which is what Encoder does.
TEST(QuickFAST, testLiveBufferHandleStillSelects)
{
  Codecs::DataDestination destination;
  const Codecs::DataDestination::BufferHandle first = putOne(destination, 'A');
  putOne(destination, 'B');

  destination.selectBuffer(first);
  destination.putByte('!');

  EXPECT_EQ("A!B", stringified(destination));
  EXPECT_EQ("A!B", gathered(destination));
}
