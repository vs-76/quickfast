// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Common/Decimal.h>
#include <Common/Exceptions.h>

#include <climits>
#include <string>

using namespace QuickFAST;

TEST(QuickFAST, testDecimalAssignmentCarriesAutoNormalize)
{
  // Assignment is copy-and-swap, and swap exchanged only the mantissa and the
  // exponent. The value came from the source while autoNormalize_ kept the
  // target's previous setting, so copy-assignment produced a different object
  // than copy-construction from the same source.
  const Decimal normalizing(0, 0, true);
  const Decimal plain(0, 0, false);

  Decimal adoptsNormalizing(0, 0, false);
  adoptsNormalizing = normalizing;
  adoptsNormalizing.parse("1.500");
  EXPECT_EQ((adoptsNormalizing.getMantissa()), (15));
  EXPECT_EQ((adoptsNormalizing.getExponent()), (-1));

  Decimal adoptsPlain(0, 0, true);
  adoptsPlain = plain;
  adoptsPlain.parse("1.500");
  EXPECT_EQ((adoptsPlain.getMantissa()), (1500));
  EXPECT_EQ((adoptsPlain.getExponent()), (-3));

  // Copy construction has always carried the flag; the two must agree.
  Decimal constructed(normalizing);
  constructed.parse("1.500");
  EXPECT_EQ((constructed.getMantissa()), (adoptsNormalizing.getMantissa()));
}

TEST(QuickFAST, testDecimalSwapExchangesEveryMember)
{
  Decimal normalizing(1, 0, true);
  Decimal plain(2, 0, false);
  normalizing.swap(plain);

  // After the swap each object must behave as the other one did.
  normalizing.parse("1.500");
  plain.parse("1.500");
  EXPECT_EQ((normalizing.getMantissa()), (1500));
  EXPECT_EQ((plain.getMantissa()), (15));
}

TEST(QuickFAST, testDecimalComparesAcrossUnreachableExponents)
{
  // denormalize() gives up when scaling the mantissa further would overflow,
  // but every comparison assumed it had reached the requested exponent and
  // compared raw mantissas anyway.
  const Decimal hundredfold(1000000000000000000LL, 2);  // 1e20
  const Decimal base(1000000000000000000LL, 0);         // 1e18
  const Decimal fiveBase(5000000000000000000LL, 0);     // 5e18

  // Reported equal, because both mantissas are 1e18 once denormalize bails.
  EXPECT_FALSE(hundredfold == base);
  EXPECT_TRUE(hundredfold != base);

  // Reported less than, though 1e20 is twenty times larger.
  EXPECT_FALSE(hundredfold < fiveBase);
  EXPECT_TRUE(fiveBase < hundredfold);

  // Ordering must stay a strict weak ordering whichever way it is asked.
  EXPECT_TRUE(hundredfold > base);
  EXPECT_FALSE(base > hundredfold);
}

TEST(QuickFAST, testDecimalDenormalizeHandlesNegativeMantissas)
{
  // The guard bounded only the positive side, so for any negative mantissa
  // "mantissa_ < LLONG_MAX/10" was trivially true and the loop multiplied
  // without limit until it overflowed. Prices are routinely negative.
  Decimal negative(-1000000000000000000LL, 2);          // -1e20
  const Decimal positive(1LL, 0);

  EXPECT_TRUE(negative < positive);
  EXPECT_FALSE(positive < negative);

  // The same overflow on the direct call. Scaling -1e18 by ten does not fit,
  // so the request cannot be honoured and the value must be left alone --
  // it used to come back as +8446744073709551616.
  EXPECT_FALSE(negative.denormalize(-2));
  EXPECT_EQ((negative.getMantissa()), (-1000000000000000000LL));
  EXPECT_EQ((negative.getExponent()), (2));
}

TEST(QuickFAST, testDecimalDivisionByZeroIsReported)
{
  // A default-constructed Decimal has a zero mantissa, so dividing by one was
  // integer division by zero: SIGFPE rather than an error a caller can handle.
  Decimal value(10, 0);
  const Decimal zero;
  EXPECT_THROW(value /= zero, OverflowError);

  // The value must be left alone when the operation is refused.
  EXPECT_EQ((value.getMantissa()), (10));
  EXPECT_EQ((value.getExponent()), (0));
}

TEST(QuickFAST, testDecimalNonStrictNormalizeDoesNotOverflow)
{
  // operator/= is the live caller of normalize(false). Its recovery loop
  // multiplied the mantissa by 10 until the exponent was back in range, with
  // no bound, so a large mantissa overflowed int64 on the way down.
  Decimal value(1, 0);
  const Decimal tiny(1, -100);
  EXPECT_THROW(value /= tiny, OverflowError);
}

TEST(QuickFAST, testDecimalMultiplyReportsExponentOverflow)
{
  // normalize permits an exponent of 64, so two operands at 64 sum to 128,
  // which does not fit in the int8_t exponent. The narrowing wrapped to -128
  // and normalize then reported an *underflow* for an overflow.
  Decimal left(1, 64);
  const Decimal right(1, 64);
  try
  {
    left *= right;
    FAIL() << "expected an exponent overflow";
  }
  catch(const OverflowError & e)
  {
    EXPECT_NE((std::string(e.what()).find("overflow")), (std::string::npos))
      << "reported as: " << e.what();
  }
}
