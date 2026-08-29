// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A differential test for the stop-bit encoding of signed integers.
//
// encodeSignedInteger is 190 lines of hand-unrolled hex with one branch per
// output length. A missing branch is invisible by inspection and produces
// well-formed FAST, so the only practical check is to compare it against an
// implementation whose shape makes a missing length impossible: the loop that
// sits beside it in FieldInstruction.cpp, disabled behind #if 0 because the
// unrolled version is about 15% faster. This test makes the loop earn its keep
// as the oracle for the version that actually ships.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/FieldInstruction.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/Decoder.h>
#include <Codecs/TemplateRegistry.h>
#include <Common/WorkingBuffer.h>

#include <random>

using namespace QuickFAST;

namespace
{
  /// @brief The reference stop-bit encoder, from the #if 0 block in FieldInstruction.cpp.
  ///
  /// It emits groups until what remains is pure sign extension and the sign of
  /// the last group agrees with the sign of the value, so it cannot omit a
  /// length: there is no length to omit.
  std::string referenceEncoding(int64 value)
  {
    std::string reversed;
    int64 until = 0;
    uchar sign = 0;
    if(value < 0)
    {
      until = -1;
      sign = signBit;
    }

    uchar prevByte = static_cast<uchar>(~sign); // force at least one group
    uchar byte = stopBit;
    while(value != until || (prevByte & signBit) != sign)
    {
      byte = static_cast<uchar>(byte | (value & dataBits));
      value >>= dataShift;
      reversed.push_back(static_cast<char>(byte));
      prevByte = byte;
      byte = 0;
    }
    return std::string(reversed.rbegin(), reversed.rend());
  }

  /// @brief What the shipping encoder puts on the wire for a value.
  std::string actualEncoding(int64 value)
  {
    Codecs::DataDestination destination;
    WorkingBuffer buffer;
    Codecs::FieldInstruction::encodeSignedInteger(destination, buffer, value);
    std::string encoded;
    destination.toString(encoded);
    return encoded;
  }

  std::string hex(const std::string & bytes)
  {
    std::string result;
    for(unsigned char byte : bytes)
    {
      char digits[4];
      std::snprintf(digits, sizeof(digits), "%02X ", byte);
      result += digits;
    }
    return result;
  }

  /// @brief Encode a value, then read it back with the real decoder.
  int64 roundTrip(int64 value)
  {
    std::string encoded = actualEncoding(value);
    Codecs::DataSourceString source(encoded);
    Codecs::Decoder decoder(Codecs::TemplateRegistryPtr(new Codecs::TemplateRegistry));
    int64 decoded = 0;
    Codecs::FieldInstruction::decodeSignedInteger(
      source, decoder, decoded, "value", true);
    return decoded;
  }

  void expectEncodesCorrectly(int64 value)
  {
    SCOPED_TRACE("value = " + std::to_string(value));
    const std::string expected = referenceEncoding(value);
    const std::string actual = actualEncoding(value);
    EXPECT_EQ(hex(expected), hex(actual));
    EXPECT_EQ(value, roundTrip(value));
  }

  /// @brief Every value at which the encoded length changes, and its neighbours.
  ///
  /// Each stop-bit group carries seven bits, so the length changes at
  /// +/- 2^(7n-1). A missing branch shows up as a length that is one group
  /// short, which is exactly what these straddle.
  std::vector<int64> lengthBoundaries()
  {
    std::vector<int64> values;
    for(int bits = 6; bits < 63; bits += 7)
    {
      const int64 threshold = int64(1) << bits;
      for(int64 offset = -2; offset <= 2; ++offset)
      {
        values.push_back(threshold + offset);
        values.push_back(-threshold + offset);
      }
    }
    values.push_back(0);
    values.push_back(1);
    values.push_back(-1);
    values.push_back(std::numeric_limits<int64>::max());
    values.push_back(std::numeric_limits<int64>::max() - 1);
    values.push_back(std::numeric_limits<int64>::min());
    values.push_back(std::numeric_limits<int64>::min() + 1);
    return values;
  }
}

/// @brief Values below -2^62 need ten groups, and the encoder only had nine.
///
/// The negative chain stopped at the eight-group threshold and its final else
/// emitted nine groups, which carry 63 signed bits. The top bits were dropped
/// and the sign bit of the truncated field landed as zero, so the value came
/// back positive: INT64_MIN + 1 was transmitted as +1. INT64_MIN itself was
/// correct only because it had a hand-written special case, which is also why
/// the one extreme value the suite exercised could not reveal this.
TEST(QuickFAST, testSignedIntegerEncodingBelowNegativeTwoToThe62)
{
  const int64 lowest = std::numeric_limits<int64>::min();
  const int64 firstCorrect = -(int64(1) << 62);
  const int64 stride = (firstCorrect - lowest) / 64;

  for(int64 value = lowest; value < firstCorrect; value += stride)
  {
    expectEncodesCorrectly(value);
  }

  // Both ends of the broken range, and the first value that always worked.
  expectEncodesCorrectly(lowest);
  expectEncodesCorrectly(lowest + 1);
  expectEncodesCorrectly(firstCorrect - 1);
  expectEncodesCorrectly(firstCorrect);
}

/// @brief The encoded length must change exactly where the seven-bit groups say.
TEST(QuickFAST, testSignedIntegerEncodingAtLengthBoundaries)
{
  for(int64 value : lengthBoundaries())
  {
    expectEncodesCorrectly(value);
  }
}

/// @brief A spread of values across the whole domain, to catch anything the
/// boundary list misses.
TEST(QuickFAST, testSignedIntegerEncodingAcrossTheDomain)
{
  std::mt19937_64 generator(20260829u);
  for(int i = 0; i < 20000; ++i)
  {
    expectEncodesCorrectly(static_cast<int64>(generator()));
  }
}
