// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// LexicalCast.h had no tests of its own, which is how it shipped less strict
// than the boost::lexical_cast it replaced. num_get accepts a leading minus
// for an unsigned target and applies the result modulo 2^N without setting
// failbit, so the stream guard never fires and "-1" becomes the largest
// representable value.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Common/LexicalCast.h>
#include <Common/Types.h>

using namespace QuickFAST;

/// @brief An unsigned target must refuse a negative value, not wrap it.
///
/// Every numeric command line option goes through here, so "-buffersize -5"
/// became a request for 2^64-5 bytes and "-buffers -1" for 2^64-1 buffers:
/// bad_alloc in the first case, an unkillable loop in the second.
TEST(QuickFAST, testLexicalCastRejectsNegativeForUnsignedTargets)
{
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("-1")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("-5")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<size_t>(std::string(" -5")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("\t-5")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<unsigned short>(std::string("-1")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<unsigned long>(std::string("-0")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<uint64>(std::string("-9223372036854775808")),
    std::invalid_argument);
}

/// @brief Valid unsigned input is unaffected.
TEST(QuickFAST, testLexicalCastAcceptsUnsignedValues)
{
  EXPECT_EQ(0u, lexical_cast<size_t>(std::string("0")));
  EXPECT_EQ(5u, lexical_cast<size_t>(std::string("5")));
  EXPECT_EQ(5u, lexical_cast<size_t>(std::string(" 5 ")));
  EXPECT_EQ(65535u, lexical_cast<unsigned short>(std::string("65535")));
  EXPECT_EQ(std::numeric_limits<uint64>::max(),
    lexical_cast<uint64>(std::string("18446744073709551615")));
}

/// @brief A signed target must still accept negative values.
TEST(QuickFAST, testLexicalCastAcceptsNegativeForSignedTargets)
{
  EXPECT_EQ(-1, lexical_cast<int>(std::string("-1")));
  EXPECT_EQ(-5, lexical_cast<int64>(std::string("-5")));
  EXPECT_EQ(std::numeric_limits<int64>::min(),
    lexical_cast<int64>(std::string("-9223372036854775808")));
}

/// @brief An out-of-range magnitude still throws, as it always did.
///
/// This was the half that worked, and it is why the function looked validated:
/// magnitude sets failbit, sign does not.
TEST(QuickFAST, testLexicalCastRejectsOverflow)
{
  EXPECT_THROW((void)lexical_cast<unsigned short>(std::string("99999")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<int>(std::string("99999999999999999999")),
    std::invalid_argument);
}

/// @brief Non-numeric and trailing-garbage input is still rejected.
TEST(QuickFAST, testLexicalCastRejectsMalformedInput)
{
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("abc")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<size_t>(std::string("5x")),
    std::invalid_argument);
  EXPECT_THROW((void)lexical_cast<int>(std::string("- 5")),
    std::invalid_argument);
}

/// @brief Conversion to string is unchanged.
TEST(QuickFAST, testLexicalCastToString)
{
  EXPECT_EQ("42", lexical_cast<std::string>(42));
  EXPECT_EQ("-42", lexical_cast<std::string>(-42));
  EXPECT_EQ("x", lexical_cast<std::string>(std::string("x")));
}
