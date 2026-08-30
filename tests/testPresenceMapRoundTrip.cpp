// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// PresenceMap encode/decode must be inverses, and checkSpecificField must not
// advance the read cursor. The existing suite covers growth, reset and buffer
// bounds; these fill the round-trip and random-access holes.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/PresenceMap.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/DataDestination.h>

using namespace QuickFAST;

/// @brief Bits written with setNextField must come back through decode(encode).
TEST(QuickFAST, testPresenceMapEncodeDecodeRoundTrip)
{
  const bool pattern[] =
  {
    true, false, true, true, false, false, true,
    false, true, false, true, false, true, true,
    true
  };
  const size_t bitCount = sizeof(pattern) / sizeof(pattern[0]);

  Codecs::PresenceMap encoded(bitCount);
  for(size_t bit = 0; bit < bitCount; ++bit)
  {
    encoded.setNextField(pattern[bit]);
  }

  Codecs::DataDestination destination;
  encoded.encode(destination);
  destination.endMessage();
  std::string wire;
  destination.toString(wire);
  ASSERT_FALSE(wire.empty());

  Codecs::DataSourceString source(wire);
  Codecs::PresenceMap decoded(1);
  decoded.decode(source);

  for(size_t bit = 0; bit < bitCount; ++bit)
  {
    EXPECT_EQ(pattern[bit], decoded.checkNextField()) << "bit " << bit;
  }
  EXPECT_FALSE(decoded.checkNextField());
}

/// @brief checkSpecificField reads without consuming checkNextField's cursor.
TEST(QuickFAST, testPresenceMapCheckSpecificFieldDoesNotAdvance)
{
  Codecs::PresenceMap pmap(14);
  for(size_t bit = 0; bit < 14; ++bit)
  {
    pmap.setNextField(bit % 2 == 0);
  }
  pmap.rewind();

  EXPECT_TRUE(pmap.checkSpecificField(0));
  EXPECT_FALSE(pmap.checkSpecificField(1));
  EXPECT_TRUE(pmap.checkSpecificField(2));
  EXPECT_FALSE(pmap.checkSpecificField(1000));

  // Cursor is still at the start.
  EXPECT_TRUE(pmap.checkNextField());
  EXPECT_FALSE(pmap.checkNextField());
}

/// @brief operator== requires a bit-for-bit match of the stored map.
TEST(QuickFAST, testPresenceMapEqualityRequiresIdenticalBits)
{
  Codecs::PresenceMap left(7);
  Codecs::PresenceMap right(7);
  for(size_t bit = 0; bit < 7; ++bit)
  {
    left.setNextField(true);
    right.setNextField(true);
  }
  EXPECT_TRUE(left == right);

  Codecs::PresenceMap different(7);
  for(size_t bit = 0; bit < 7; ++bit)
  {
    different.setNextField(bit != 3);
  }
  EXPECT_FALSE(left == different);
}
