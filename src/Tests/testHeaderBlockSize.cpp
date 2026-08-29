// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// The FAST-encoded header's block size loop shifted left seven bits per byte
// until a stop bit, with no bound on the iteration count and no overflow
// detection. Ten continuation bytes exhaust a 64-bit size_t and anything after
// that silently discards the high bits, so a sender chooses the value freely
// across its whole range -- including the values that wedge StreamingAssembler
// waiting for a block that can never arrive.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/FastEncodedHeaderAnalyzer.h>
#include <Codecs/DataSourceString.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  /// @brief Encode a value the way a FAST-encoded header carries it.
  std::string sevenBitEncoded(uint64 value)
  {
    std::string reversed;
    reversed.push_back(static_cast<char>((value & 0x7f) | 0x80));
    value >>= 7;
    while(value != 0)
    {
      reversed.push_back(static_cast<char>(value & 0x7f));
      value >>= 7;
    }
    return std::string(reversed.rbegin(), reversed.rend());
  }

  size_t blockSizeOf(const std::string & header)
  {
    Codecs::FastEncodedHeaderAnalyzer analyzer(0, 0, true);
    Codecs::DataSourceString source(header);
    size_t blockSize = 0;
    bool skip = false;
    EXPECT_TRUE(analyzer.analyzeHeader(source, blockSize, skip));
    return blockSize;
  }
}

/// @brief Ordinary block sizes are unaffected.
TEST(QuickFAST, testFastEncodedHeaderReadsBlockSize)
{
  EXPECT_EQ(0u, blockSizeOf(sevenBitEncoded(0)));
  EXPECT_EQ(1u, blockSizeOf(sevenBitEncoded(1)));
  EXPECT_EQ(127u, blockSizeOf(sevenBitEncoded(127)));
  EXPECT_EQ(128u, blockSizeOf(sevenBitEncoded(128)));
  EXPECT_EQ(1500u, blockSizeOf(sevenBitEncoded(1500)));
  EXPECT_EQ(0xFFFFFFFFu, blockSizeOf(sevenBitEncoded(0xFFFFFFFF)));
}

/// @brief The largest value a size_t can hold still reads back exactly.
TEST(QuickFAST, testFastEncodedHeaderReadsMaximumBlockSize)
{
  const uint64 maximum = std::numeric_limits<uint64>::max();
  EXPECT_EQ(maximum, blockSizeOf(sevenBitEncoded(maximum)));
}

/// @brief A block size with more bits than a size_t must be refused.
///
/// Eleven groups is one more than a 64-bit value can use. Silently dropping
/// the high bits hands the sender an arbitrary block size.
TEST(QuickFAST, testFastEncodedHeaderRejectsOverlongBlockSize)
{
  std::string header(10, '\x01');
  header.push_back('\x81');
  EXPECT_THROW((void)blockSizeOf(header), OverflowError);
}

/// @brief A run of continuation bytes must not be read forever.
TEST(QuickFAST, testFastEncodedHeaderRejectsUnterminatedBlockSize)
{
  const std::string header(64, '\x7f');
  EXPECT_THROW((void)blockSizeOf(header), OverflowError);
}

/// @brief Ten groups whose leading bits overflow must be refused too.
///
/// Ten groups can carry seventy bits, so the count alone is not the whole
/// check: the top group must fit in what remains of the size_t.
TEST(QuickFAST, testFastEncodedHeaderRejectsOverflowInTheTopGroup)
{
  // 0x02 in the first of ten groups puts a bit at position 65.
  std::string header;
  header.push_back('\x02');
  header.append(8, '\x00');
  header.push_back('\x80');
  EXPECT_THROW((void)blockSizeOf(header), OverflowError);
}
