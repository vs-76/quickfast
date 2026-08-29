// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Common/Decimal.h>

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
