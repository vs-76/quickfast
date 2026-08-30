// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// FieldSet identity lookup: first-match semantics, and (with test hooks) that
// in-order encode-style access is amortized O(1) comparisons per field rather
// than the classic O(F) scan from index 0 on every call.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/FieldSet.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldIdentity.h>
#include <vector>
#include <string>

using namespace QuickFAST;

TEST(QuickFAST, testFieldSetLookupFirstMatchAndAbsent)
{
  // Stable identities: MessageField borrows references.
  static const Messages::FieldIdentity a("a");
  static const Messages::FieldIdentity b("b");
  static const Messages::FieldIdentity missing("missing");

  Messages::FieldSet fields(4);
  fields.addField(a, Messages::FieldUInt32::create(1));
  fields.addField(b, Messages::FieldUInt32::create(2));

  Messages::FieldCPtr value;
  EXPECT_TRUE(fields.getField(a, value));
  EXPECT_EQ(1u, value->toUInt32());
  EXPECT_TRUE(fields.getField(b, value));
  EXPECT_EQ(2u, value->toUInt32());
  EXPECT_FALSE(fields.getField(missing, value));
  EXPECT_TRUE(fields.isPresent(a));
  EXPECT_FALSE(fields.isPresent(missing));
}

TEST(QuickFAST, testFieldSetLookupDuplicateFirstMatch)
{
  static const Messages::FieldIdentity dup("dup");
  static const Messages::FieldIdentity other("other");

  Messages::FieldSet fields(4);
  fields.addField(dup, Messages::FieldUInt32::create(10));
  fields.addField(other, Messages::FieldUInt32::create(20));
  fields.addField(dup, Messages::FieldUInt32::create(30));

  Messages::FieldCPtr value;
  ASSERT_TRUE(fields.getField(dup, value));
  EXPECT_EQ(10u, value->toUInt32());

  // replaceField must update the first match, not a later duplicate.
  EXPECT_TRUE(fields.replaceField(dup, Messages::FieldUInt32::create(11)));
  ASSERT_TRUE(fields.getField(dup, value));
  EXPECT_EQ(11u, value->toUInt32());
  EXPECT_EQ(11u, fields[0].getField()->toUInt32());
  EXPECT_EQ(30u, fields[2].getField()->toUInt32());
}

TEST(QuickFAST, testFieldSetLookupOutOfOrderAndClear)
{
  static const Messages::FieldIdentity a("a");
  static const Messages::FieldIdentity b("b");
  static const Messages::FieldIdentity c("c");

  Messages::FieldSet fields(4);
  fields.addField(a, Messages::FieldUInt32::create(1));
  fields.addField(b, Messages::FieldUInt32::create(2));
  fields.addField(c, Messages::FieldUInt32::create(3));

  Messages::FieldCPtr value;
  // Reverse order must still resolve correctly.
  EXPECT_TRUE(fields.getField(c, value));
  EXPECT_EQ(3u, value->toUInt32());
  EXPECT_TRUE(fields.getField(a, value));
  EXPECT_EQ(1u, value->toUInt32());
  EXPECT_TRUE(fields.getField(b, value));
  EXPECT_EQ(2u, value->toUInt32());

  fields.clear();
  EXPECT_FALSE(fields.getField(a, value));
  fields.addField(a, Messages::FieldUInt32::create(9));
  EXPECT_TRUE(fields.getField(a, value));
  EXPECT_EQ(9u, value->toUInt32());
}

TEST(QuickFAST, testFieldSetLookupInterleavedIsPresent)
{
  static const Messages::FieldIdentity a("a");
  static const Messages::FieldIdentity b("b");
  static const Messages::FieldIdentity skip("skip");

  Messages::FieldSet fields(4);
  fields.addField(a, Messages::FieldUInt32::create(1));
  fields.addField(b, Messages::FieldUInt32::create(2));

  EXPECT_FALSE(fields.isPresent(skip));
  EXPECT_TRUE(fields.isPresent(a));
  EXPECT_FALSE(fields.isPresent(skip));
  EXPECT_TRUE(fields.isPresent(b));

  Messages::FieldCPtr value;
  EXPECT_TRUE(fields.getField(a, value));
  EXPECT_EQ(1u, value->toUInt32());
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
TEST(QuickFAST, testFieldSetInOrderLookupIsAmortizedLinear)
{
  // Prove the encode-shaped access pattern (fields looked up in insertion order)
  // costs O(F) identity comparisons, not O(F^2). A naive scan-from-zero does
  // 1+2+...+F = F*(F+1)/2 comparisons for the same pattern.
  const size_t F = 64;
  std::vector<Messages::FieldIdentity> ids;
  ids.reserve(F);
  for(size_t i = 0; i < F; ++i)
  {
    ids.emplace_back("f" + std::to_string(i));
  }

  Messages::FieldSet fields(F);
  for(size_t i = 0; i < F; ++i)
  {
    fields.addField(ids[i], Messages::FieldUInt32::create(static_cast<uint32>(i)));
  }

  Messages::FieldSet::resetIdentityCompareCount();
  Messages::FieldCPtr value;
  for(size_t i = 0; i < F; ++i)
  {
    ASSERT_TRUE(fields.getField(ids[i], value));
    EXPECT_EQ(static_cast<uint32>(i), value->toUInt32());
  }
  const uint64_t compares = Messages::FieldSet::identityCompareCount();
  // Hint hit: one compare per field. Allow a small constant slack.
  EXPECT_LE(compares, F + 2u);
  EXPECT_LT(compares, (F * (F + 1)) / 2);
}

TEST(QuickFAST, testFieldSetBuildCostsNoIdentityCompares)
{
  // Building a message is the decode path. Duplicate detection belongs to
  // lookup, so adding F fields must not compare identities at all -- doing it
  // per add costs O(F^2) three-way string compares on every decoded message.
  const size_t F = 64;
  std::vector<Messages::FieldIdentity> ids;
  ids.reserve(F);
  for(size_t i = 0; i < F; ++i)
  {
    ids.emplace_back("f" + std::to_string(i));
  }

  Messages::FieldSet fields(F);
  Messages::FieldSet::resetIdentityCompareCount();
  for(size_t i = 0; i < F; ++i)
  {
    fields.addField(ids[i], Messages::FieldUInt32::create(static_cast<uint32>(i)));
  }
  EXPECT_EQ(0u, Messages::FieldSet::identityCompareCount());

  // The flag still has to be right once somebody does look something up.
  Messages::FieldCPtr value;
  ASSERT_TRUE(fields.getField(ids[F - 1], value));
  EXPECT_EQ(static_cast<uint32>(F - 1), value->toUInt32());
}

TEST(QuickFAST, testFieldSetDuplicateDetectedAfterLateAdd)
{
  // A duplicate added after lookups have already run must still be honored:
  // the cached "no duplicates" answer has to be invalidated by addField.
  Messages::FieldIdentity a("dup");
  Messages::FieldIdentity b("other");
  Messages::FieldIdentity later("dup");

  Messages::FieldSet fields(4);
  fields.addField(a, Messages::FieldUInt32::create(1));
  fields.addField(b, Messages::FieldUInt32::create(2));

  Messages::FieldCPtr value;
  ASSERT_TRUE(fields.getField(b, value));
  EXPECT_EQ(2u, value->toUInt32());

  fields.addField(later, Messages::FieldUInt32::create(3));

  // First match wins, even though the cursor is sitting past the first entry.
  ASSERT_TRUE(fields.getField(later, value));
  EXPECT_EQ(1u, value->toUInt32());
  ASSERT_TRUE(fields.getField(a, value));
  EXPECT_EQ(1u, value->toUInt32());
}
#endif
