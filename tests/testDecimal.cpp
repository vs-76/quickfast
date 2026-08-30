// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Common/Decimal.h>
#include <Common/Exceptions.h>

#include <Codecs/DataSourceString.h>
#include <Codecs/Decoder.h>
#include <Codecs/DictionaryIndexer.h>
#include <Codecs/FieldInstructionDecimal.h>
#include <Codecs/FieldOpDelta.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/PresenceMap.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/TemplateRegistry.h>
#include <Messages/Message.h>

#include <climits>
#include <string>
#include <vector>

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

namespace
{
  // Decode a decimal field carrying a <delta> operator straight from wire
  // bytes, the way a hostile counterparty would deliver it. Successive wire
  // strings share one decoder, so each delta applies to the value the previous
  // one left in the dictionary.
  Decimal decodeDecimalDeltas(const std::vector<std::string> & messages)
  {
    Codecs::DictionaryIndexer indexer;
    Codecs::FieldInstructionDecimal field("Value", "");
    Codecs::FieldOpPtr fieldOp(new Codecs::FieldOpDelta);
    field.setFieldOp(fieldOp);
    field.indexDictionaries(indexer, "global", "", "");
    Codecs::TemplateRegistryPtr registry(
      new Codecs::TemplateRegistry(3, 3, indexer.size()));
    field.finalize(*registry);

    Codecs::Decoder decoder(registry);
    Decimal result;
    for(const std::string & wire : messages)
    {
      Codecs::DataSourceString source(wire);
      Codecs::PresenceMap pmap(1);
      Codecs::SingleMessageConsumer consumer;
      Codecs::GenericMessageBuilder builder(consumer);

      builder.startMessage("UNIT_TEST", "", 10);
      field.decode(source, pmap, decoder, builder);
      builder.endMessage(builder);

      Messages::Message & fieldSet = consumer.message();
      if(fieldSet.size() != 1)
      {
        throw std::runtime_error("decode produced no field");
      }
      result = fieldSet.begin()->getField()->toDecimal();
    }
    return result;
  }

  Decimal decodeDecimalDelta(const std::string & wire)
  {
    return decodeDecimalDeltas(std::vector<std::string>(1, wire));
  }

  // A signed FAST integer holding INT64_MAX: nine all-ones septets, with a
  // leading zero septet so the sign bit does not make it negative.
  const std::string int64MaxDelta("\x00\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\xff", 10);
}

TEST(QuickFAST, testDecimalDeltaRejectsAnOutOfRangeExponent)
{
  // A decimal delta is an exponent delta followed by a mantissa delta, each a
  // stop-bit encoded signed integer. getExponent() returns int8_t and the
  // delta is int64, so the sum was computed at full width and then narrowed by
  // an explicit exponent_t cast: a delta of 200 against an exponent of 0 gave
  // int8_t(200) == -56, putting the value out by a factor of 10^56 with no
  // diagnostic. Being an explicit cast, no sanitizer reports it.
  //
  // 200 as a signed FAST integer is 0x01 0xC8; the mantissa delta is zero.
  EXPECT_THROW(
    decodeDecimalDelta(std::string("\x01\xC8\x80", 3)),
    EncodingError);

  // The FAST spec confines exponents to [-63, 63]; 64 is already out.
  EXPECT_THROW(
    decodeDecimalDelta(std::string("\x00\xC0\x80", 3)),
    EncodingError);

  // A delta that lands inside the legal range still decodes.
  const Decimal value = decodeDecimalDelta(std::string("\x83\x85", 2));
  EXPECT_EQ((value.getExponent()), (3));
  EXPECT_EQ((value.getMantissa()), (5));
}

TEST(QuickFAST, testDecimalDeltaRejectsAMantissaThatOverflows)
{
  // value.getMantissa() + mantissaDelta is added as two int64 values before
  // the mantissa_t cast, so the cast cannot rescue it: a large stored mantissa
  // plus a large wire delta is signed overflow -- undefined behaviour, not a
  // wrong number anything downstream could notice.
  //
  // The first delta parks INT64_MAX in the dictionary; the second adds one.
  std::vector<std::string> messages;
  messages.push_back(std::string("\x80", 1) + int64MaxDelta);
  messages.push_back(std::string("\x80\x81", 2));
  EXPECT_THROW(decodeDecimalDeltas(messages), EncodingError);

  // Reaching INT64_MAX on its own is legal and must still work.
  std::vector<std::string> justTheMaximum;
  justTheMaximum.push_back(std::string("\x80", 1) + int64MaxDelta);
  const Decimal value = decodeDecimalDeltas(justTheMaximum);
  EXPECT_EQ((value.getMantissa()), (LLONG_MAX));
}

TEST(QuickFAST, testDecimalParseRejectsTooManyFractionalDigits)
{
  // fracString.size() is a size_t narrowed to int8_t, so 200 fractional
  // digits gave int8_t(200) == -56, negated to +56: the parsed number was out
  // by roughly 10^112 and the sign of the exponent was inverted.
  const std::string tooPrecise = "0." + std::string(200, '1');
  Decimal value;
  EXPECT_THROW(value.parse(tooPrecise), OverflowError);

  // The largest exponent a Decimal can hold still parses.
  Decimal legal;
  EXPECT_NO_THROW(legal.parse("0." + std::string(8, '1')));
  EXPECT_EQ((legal.getExponent()), (-8));
}

TEST(QuickFAST, testDecimalParseRejectsMalformedInput)
{
  // The first lexical_cast is guarded and sets the overflow flag; the recovery
  // path ran a second, unguarded one. Input that is malformed rather than
  // merely too large reached it and threw bad_lexical_cast straight out of a
  // void function with no documented exception contract.
  Decimal value;
  EXPECT_THROW(value.parse("-"), UsageError);
  EXPECT_THROW(value.parse("1.2.3"), UsageError);
  EXPECT_THROW(value.parse(""), UsageError);
  EXPECT_THROW(value.parse("abc"), UsageError);
  EXPECT_THROW(value.parse("1e5"), UsageError);

  // Ordinary input is unaffected, including the forms that were already fine.
  Decimal ok;
  ok.parse("  -12.750  ");
  EXPECT_EQ((double(ok)), (-12.75));
  ok.parse("42");
  EXPECT_EQ((double(ok)), (42.0));
  ok.parse("+3.5");
  EXPECT_EQ((double(ok)), (3.5));
}

TEST(QuickFAST, testDecimalParseHandlesAnOversizeMantissa)
{
  // The recovery path trims trailing zeros to make an over-long mantissa fit.
  Decimal value;
  value.parse("1" + std::string(25, '0'));
  EXPECT_EQ((value.getMantissa()), (1));
  EXPECT_EQ((value.getExponent()), (25));

  // Too many significant digits cannot be recovered by trimming zeros.
  Decimal hopeless;
  EXPECT_THROW(hopeless.parse(std::string(25, '9')), OverflowError);
}
