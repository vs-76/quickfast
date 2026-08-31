// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Contiguous vs byte-at-a-time decode for int64/uint64 must agree, including
// values that straddle artificial buffer boundaries so the fallback path runs.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/FieldInstruction.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSource.h>
#include <Codecs/DataSourceBuffer.h>
#include <Codecs/Decoder.h>
#include <Codecs/TemplateRegistry.h>
#include <Common/WorkingBuffer.h>
#include <Common/Exceptions.h>

#include <string>
#include <vector>
#include <limits>
#include <algorithm>

using namespace QuickFAST;

namespace
{
  /// @brief Yields one byte per getBuffer so hasContiguous(n) fails for n > 1.
  class OneByteSource : public Codecs::DataSource
  {
  public:
    explicit OneByteSource(std::string bytes)
      : storage_(std::move(bytes))
      , next_(0)
    {
    }

    bool getBuffer(const uchar *& buffer, size_t & size) override
    {
      if(next_ >= storage_.size())
      {
        return false;
      }
      buffer = reinterpret_cast<const uchar *>(storage_.data()) + next_;
      size = 1;
      ++next_;
      return true;
    }

  private:
    std::string storage_;
    size_t next_;
  };

  /// @brief Two buffers: prefix of splitAt bytes, then the remainder (exercises straddling).
  class SplitSource : public Codecs::DataSource
  {
  public:
    SplitSource(std::string bytes, size_t splitAt)
      : storage_(std::move(bytes))
      , splitAt_(std::min(splitAt, storage_.size()))
      , stage_(0)
    {
    }

    bool getBuffer(const uchar *& buffer, size_t & size) override
    {
      while(stage_ < 2)
      {
        if(stage_ == 0)
        {
          buffer = reinterpret_cast<const uchar *>(storage_.data());
          size = splitAt_;
          ++stage_;
          if(size > 0)
          {
            return true;
          }
          continue;
        }
        buffer = reinterpret_cast<const uchar *>(storage_.data()) + splitAt_;
        size = storage_.size() - splitAt_;
        ++stage_;
        return size > 0;
      }
      return false;
    }

  private:
    std::string storage_;
    size_t splitAt_;
    int stage_;
  };

  std::string encodeSigned(int64 value)
  {
    Codecs::DataDestination destination;
    WorkingBuffer buffer;
    Codecs::FieldInstruction::encodeSignedInteger(destination, buffer, value);
    std::string encoded;
    destination.toString(encoded);
    return encoded;
  }

  std::string encodeUnsigned(uint64 value)
  {
    Codecs::DataDestination destination;
    WorkingBuffer buffer;
    Codecs::FieldInstruction::encodeUnsignedInteger(destination, buffer, value);
    std::string encoded;
    destination.toString(encoded);
    return encoded;
  }

  Codecs::Decoder & unusedContext()
  {
    static Codecs::Decoder decoder(Codecs::TemplateRegistryPtr(new Codecs::TemplateRegistry));
    return decoder;
  }

  int64 decodeSignedContiguous(const std::string & encoded)
  {
    Codecs::DataSourceBuffer source(
      reinterpret_cast<const unsigned char *>(encoded.data()), encoded.size());
    int64 value = 0;
    Codecs::FieldInstruction::decodeSignedInteger(
      source, unusedContext(), value, "i64", true);
    return value;
  }

  int64 decodeSignedOneByte(const std::string & encoded)
  {
    OneByteSource source(encoded);
    int64 value = 0;
    Codecs::FieldInstruction::decodeSignedInteger(
      source, unusedContext(), value, "i64", true);
    return value;
  }

  uint64 decodeUnsignedContiguous(const std::string & encoded)
  {
    Codecs::DataSourceBuffer source(
      reinterpret_cast<const unsigned char *>(encoded.data()), encoded.size());
    uint64 value = 0;
    Codecs::FieldInstruction::decodeUnsignedInteger(
      source, unusedContext(), value, "u64");
    return value;
  }

  uint64 decodeUnsignedOneByte(const std::string & encoded)
  {
    OneByteSource source(encoded);
    uint64 value = 0;
    Codecs::FieldInstruction::decodeUnsignedInteger(
      source, unusedContext(), value, "u64");
    return value;
  }

  std::vector<int64> signedCases()
  {
    return {
      0, 1, -1, 63, 64, -64, -65,
      127, 128, -128, -129,
      int64(1) << 31, -(int64(1) << 31), (int64(1) << 31) - 1,
      int64(1) << 32, -(int64(1) << 32),
      std::numeric_limits<int64>::max(),
      std::numeric_limits<int64>::min(),
      std::numeric_limits<int64>::max() - 1,
      std::numeric_limits<int64>::min() + 1,
    };
  }

  std::vector<uint64> unsignedCases()
  {
    return {
      0u, 1u, 127u, 128u,
      uint64(1) << 31, (uint64(1) << 32) - 1, uint64(1) << 32,
      uint64(1) << 63, std::numeric_limits<uint64>::max(),
      std::numeric_limits<uint64>::max() - 1,
    };
  }
}

TEST(QuickFAST, testInt64ContiguousMatchesByteAtATime)
{
  for(int64 value : signedCases())
  {
    SCOPED_TRACE(value);
    const std::string encoded = encodeSigned(value);
    EXPECT_EQ(value, decodeSignedContiguous(encoded));
    EXPECT_EQ(value, decodeSignedOneByte(encoded));
    EXPECT_EQ(decodeSignedContiguous(encoded), decodeSignedOneByte(encoded));
  }
}

TEST(QuickFAST, testUInt64ContiguousMatchesByteAtATime)
{
  for(uint64 value : unsignedCases())
  {
    SCOPED_TRACE(value);
    const std::string encoded = encodeUnsigned(value);
    EXPECT_EQ(value, decodeUnsignedContiguous(encoded));
    EXPECT_EQ(value, decodeUnsignedOneByte(encoded));
    EXPECT_EQ(decodeUnsignedContiguous(encoded), decodeUnsignedOneByte(encoded));
  }
}

TEST(QuickFAST, testInt64DecodeSurvivesBufferSplit)
{
  const int64 value = (int64(1) << 40) | 0x12345;
  const std::string encoded = encodeSigned(value);
  ASSERT_GT(encoded.size(), 1u);

  for(size_t split = 1; split < encoded.size(); ++split)
  {
    SCOPED_TRACE(split);
    SplitSource source(encoded, split);
    int64 decoded = 0;
    Codecs::FieldInstruction::decodeSignedInteger(
      source, unusedContext(), decoded, "i64", true);
    EXPECT_EQ(value, decoded);
  }
}

TEST(QuickFAST, testUInt64DecodeSurvivesBufferSplit)
{
  const uint64 value = (uint64(1) << 40) | 0xABCDu;
  const std::string encoded = encodeUnsigned(value);
  ASSERT_GT(encoded.size(), 1u);

  for(size_t split = 1; split < encoded.size(); ++split)
  {
    SCOPED_TRACE(split);
    SplitSource source(encoded, split);
    uint64 decoded = 0;
    Codecs::FieldInstruction::decodeUnsignedInteger(
      source, unusedContext(), decoded, "u64");
    EXPECT_EQ(value, decoded);
  }
}
