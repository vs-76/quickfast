// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>

#include <Messages/FieldIdentity.h>
#include <Messages/FieldSet.h>
#include <Messages/FieldUInt32.h>

using namespace QuickFAST;
using namespace QuickFAST::Messages;

TEST(QuickFAST, testFieldIdentityEqualityIsTransitive)
{
  // Three identities sharing a name and namespace, differing only in id.
  // The lenient rule -- ignore ids unless both are present -- made the
  // anonymous one equal to both of the others while those two were unequal,
  // so equality depended on which pair you asked about.
  const FieldIdentity anonymous("price", "ns");
  const FieldIdentity one("price", "ns", "1");
  const FieldIdentity two("price", "ns", "2");

  ASSERT_FALSE(one == two);
  EXPECT_FALSE(anonymous == one);
  EXPECT_FALSE(anonymous == two);

  // Reflexive, symmetric, and transitive: what a container is entitled to.
  const FieldIdentity sameAsOne("price", "ns", "1");
  EXPECT_TRUE(one == one);
  EXPECT_TRUE(one == sameAsOne);
  EXPECT_TRUE(sameAsOne == one);

  // Name and namespace still matter.
  EXPECT_FALSE((FieldIdentity("price", "a", "1") == FieldIdentity("price", "b", "1")));
  EXPECT_FALSE((FieldIdentity("bid", "ns", "1") == one));
}

TEST(QuickFAST, testFieldIdentityMatchesIgnoresAnAbsentId)
{
  // The lenient rule is what lookups want, so it survives under its own name.
  const FieldIdentity anonymous("price", "ns");
  const FieldIdentity one("price", "ns", "1");
  const FieldIdentity two("price", "ns", "2");

  EXPECT_TRUE(anonymous.matches(one));
  EXPECT_TRUE(one.matches(anonymous));
  EXPECT_FALSE(one.matches(two));
  EXPECT_FALSE((FieldIdentity("bid", "ns").matches(one)));
}

TEST(QuickFAST, testFieldSetLooksUpByNameWithoutAnId)
{
  // A caller that knows only the name must still find a field stored with an
  // id: this is the behaviour the lenient operator existed to provide.
  const FieldIdentity stored("price", "ns", "1");
  FieldSet fields(2);
  fields.addField(stored, FieldUInt32::create(42));

  FieldCPtr value;
  ASSERT_TRUE(fields.getField(FieldIdentity("price", "ns"), value));
  EXPECT_EQ((value->toUInt32()), (42u));
  EXPECT_TRUE(fields.isPresent(FieldIdentity("price", "ns")));

  // A different id is a different field.
  EXPECT_FALSE(fields.getField(FieldIdentity("price", "ns", "2"), value));
}
