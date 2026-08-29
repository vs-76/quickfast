// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// The block size in a fixed-size header is assembled arithmetically, byte by
// byte, with shifts and ORs. Those are defined on values rather than on memory
// layout, so the host's byte order has nothing to do with it -- the only
// question is which wire byte is most significant, which the constructor is
// already told. Consulting ByteSwapper::isBigEndian() as well inverted the
// choice on every little-endian host, which in practice means always.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>
#include <Codecs/DataSourceString.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  /// @brief Parse one header, returning the block size it declared.
  size_t blockSizeOf(
    const std::string & header,
    size_t sizeBytes,
    bool bigEndian,
    size_t prefixBytes = 0,
    size_t suffixBytes = 0)
  {
    Codecs::FixedSizeHeaderAnalyzer analyzer(
      sizeBytes, bigEndian, prefixBytes, suffixBytes);
    Codecs::DataSourceString source(header);
    size_t blockSize = 0;
    bool skip = false;
    EXPECT_TRUE(analyzer.analyzeHeader(source, blockSize, skip));
    return blockSize;
  }
}

/// @brief A big-endian size field must be read most significant byte first.
TEST(QuickFAST, testFixedSizeHeaderBigEndian)
{
  EXPECT_EQ(256u, blockSizeOf(std::string("\x00\x00\x01\x00", 4), 4, true));
  EXPECT_EQ(1u, blockSizeOf(std::string("\x00\x00\x00\x01", 4), 4, true));
  EXPECT_EQ(0x01020304u,
    blockSizeOf(std::string("\x01\x02\x03\x04", 4), 4, true));
  EXPECT_EQ(0x0102u, blockSizeOf(std::string("\x01\x02", 2), 2, true));
}

/// @brief A little-endian size field must be read least significant byte first.
TEST(QuickFAST, testFixedSizeHeaderLittleEndian)
{
  EXPECT_EQ(256u, blockSizeOf(std::string("\x00\x01\x00\x00", 4), 4, false));
  EXPECT_EQ(1u, blockSizeOf(std::string("\x01\x00\x00\x00", 4), 4, false));
  EXPECT_EQ(0x04030201u,
    blockSizeOf(std::string("\x01\x02\x03\x04", 4), 4, false));
  EXPECT_EQ(0x0201u, blockSizeOf(std::string("\x01\x02", 2), 2, false));
}

/// @brief The high bit of the most significant byte must not sign-extend.
///
/// next & 0xFF promotes to int, so << 24 on a byte with the high bit set
/// produced a negative int, which then sign-extended across the top 32 bits of
/// the size_t. A size field of 0xFF000000 came out as 0xFFFFFFFFFF000000:
/// four gigabytes reported as eighteen exabytes.
TEST(QuickFAST, testFixedSizeHeaderDoesNotSignExtend)
{
  EXPECT_EQ(0xFF000000u,
    blockSizeOf(std::string("\xff\x00\x00\x00", 4), 4, true));
  EXPECT_EQ(0xFF000000u,
    blockSizeOf(std::string("\x00\x00\x00\xff", 4), 4, false));
  EXPECT_EQ(0xFFFFFFFFu,
    blockSizeOf(std::string("\xff\xff\xff\xff", 4), 4, true));
  EXPECT_EQ(0x80u, blockSizeOf(std::string("\x80", 1), 1, false));
}

/// @brief An eight byte size field is a legal configuration.
///
/// The int shift reached 32 at the fifth byte, which is undefined behaviour
/// rather than a modular wrap. sizeBytes_ had no validation of any kind.
TEST(QuickFAST, testFixedSizeHeaderEightByteSize)
{
  EXPECT_EQ(0x0102030405060708ull,
    blockSizeOf(std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8), 8, true));
  EXPECT_EQ(0x0807060504030201ull,
    blockSizeOf(std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8), 8, false));
}

/// @brief A size field wider than size_t must be refused, not shifted off.
TEST(QuickFAST, testFixedSizeHeaderRejectsOversizedSizeField)
{
  EXPECT_THROW(
    Codecs::FixedSizeHeaderAnalyzer(sizeof(size_t) + 1, true, 0, 0),
    UsageError);
}

/// @brief Prefix and suffix bytes are skipped, not counted into the size.
TEST(QuickFAST, testFixedSizeHeaderSkipsPrefixAndSuffix)
{
  EXPECT_EQ(0x0102u,
    blockSizeOf(std::string("\xaa\xbb\x01\x02\xcc", 5), 2, true, 2, 1));
}
