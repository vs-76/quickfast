// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include <gtest/gtest.h>
#include <Common/Types.h>
#include <Codecs/PresenceMap.h>
#include <Codecs/DataSourceString.h>
#include <Codecs/DataDestination.h>

using namespace QuickFAST;

TEST(QuickFAST, testPmapDecoding)
{
  Codecs::PresenceMap pmap(1);
  for(size_t n = 0; n < 10; ++n) // more than 7 anyway
  {
    EXPECT_TRUE(!pmap.checkNextField());
  }

  uchar oneBytePMAP[] = {static_cast<uchar>(0xFFu)};
  pmap.setRaw(oneBytePMAP, 1);

  for(size_t n = 0; n < 7; ++n) // The first 7 should be present
  {
    EXPECT_TRUE(pmap.checkNextField());
  }

  for(size_t n = 0; n < 10; ++n) // after 7; MIA
  {
    EXPECT_TRUE(!pmap.checkNextField());
  }
  pmap.rewind();
  // Results should be reproducable
  for(size_t n = 0; n < 7; ++n) // The first 7 should be present
  {
    EXPECT_TRUE(pmap.checkNextField());
  }

  for(size_t n = 0; n < 10; ++n) // after 7; MIA
  {
    EXPECT_TRUE(!pmap.checkNextField());
  }

  uchar everyOtherOne[] = {static_cast<uchar>(0x55u), static_cast<uchar>(0xAAu)};
  EXPECT_EQ((sizeof(everyOtherOne)), (2));
  pmap.setRaw(everyOtherOne, sizeof(everyOtherOne));
  size_t ones = 0;
  for(size_t n = 0; n < 20; ++n)
  {
    if(pmap.checkNextField())
    {
      ++ones;
    }
  }
  EXPECT_EQ((7), (ones));

  // had a problem in calculating the buffer size
  Codecs::PresenceMap pmap197(197);
  const size_t needed197 = 29;
  const uchar * raw197 = 0;
  size_t size197 = 0;
  pmap197.getRaw(raw197, size197);
  EXPECT_EQ((needed197), (size197));
  for(size_t n = 0; n < 197; ++n)
  {
    pmap197.setNextField(true);
  }
  EXPECT_EQ((needed197), (pmap197.encodeBytesNeeded()));
  pmap197.getRaw(raw197, size197);
  EXPECT_EQ((needed197), (size197));


  // This case caused a failure
  std::string testString("\x6f\x62\xa0");
  Codecs::DataSourceString source(testString);
  Codecs::PresenceMap p1(1); // make it grow
  p1.decode(source);
  EXPECT_TRUE(p1.checkNextField()); // 6 110
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

  EXPECT_TRUE(p1.checkNextField()); // F 1111
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(p1.checkNextField());

  EXPECT_TRUE(p1.checkNextField()); // 6 110
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

  EXPECT_TRUE(!p1.checkNextField()); // 2 0010
  EXPECT_TRUE(!p1.checkNextField());
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

  EXPECT_TRUE(!p1.checkNextField()); // A 010
  EXPECT_TRUE(p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

  EXPECT_TRUE(!p1.checkNextField()); // 0
  EXPECT_TRUE(!p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

  EXPECT_TRUE(!p1.checkNextField()); // and to be sure: check past the end
  EXPECT_TRUE(!p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());
  EXPECT_TRUE(!p1.checkNextField());

}

TEST(QuickFAST, testPmapEncoding1)
{
  Codecs::PresenceMap pmap(10);
  for(size_t nbit = 0; nbit < 10; ++nbit)
  {
    pmap.setNextField(true);
  }
  Codecs::DataDestination destination;
  pmap.encode(destination);
  destination.endMessage();
  std::string result;
  destination.toString(result);
  destination.clear();
  EXPECT_EQ((result.length()), (2));
  const char expected[] = "\x7F\xF0";
  EXPECT_TRUE(result == expected);
}

TEST(QuickFAST, testPmapEncoding2)
{
  Codecs::PresenceMap pmap(10);
  for(size_t nbit = 0; nbit < 10; ++nbit)
  {
    pmap.setNextField(false);
  }
  Codecs::DataDestination destination;
  pmap.encode(destination);
  destination.endMessage();
  std::string result;
  destination.toString(result);
  destination.clear();
  EXPECT_EQ((result.length()), (1));
  const char expected[] = "\x80";
  EXPECT_TRUE(result == expected);
}
