// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <filesystem>

#include <Communication/SingleServerBufferQueue.h>
#include <Common/StringBuffer.h>
#include <Common/WorkingBuffer.h>
#include <Common/Exceptions.h>
#include <Common/Decimal.h>

using namespace QuickFAST;
TEST(QuickFAST, TestLinkedBuffer)
{
  // construct a root buffer
  Communication::LinkedBuffer root;
  EXPECT_TRUE(root.capacity() == 0);
  EXPECT_TRUE(root.used() == 0);
  EXPECT_TRUE(root.link() == 0);
  EXPECT_TRUE(root.get() == 0);

  Communication::LinkedBuffer buffer1(100);
  EXPECT_TRUE(buffer1.capacity() == 100);
  EXPECT_TRUE(buffer1.used() == 0);
  EXPECT_TRUE(buffer1.link() == 0);
  EXPECT_TRUE(buffer1.get() != 0);

  root.link(&buffer1);
  EXPECT_TRUE(root.link() == &buffer1);
  EXPECT_TRUE(buffer1.link() == 0);

  for(size_t i = 0; i < buffer1.capacity(); ++i)
  {
    buffer1[i] = 1;
  }
  buffer1.setUsed(20);
  EXPECT_TRUE(buffer1.used() == 20);

  Communication::LinkedBuffer external;
  unsigned char * hello = (unsigned char *)"hello";
  external.setExternal(hello, sizeof(hello));
  EXPECT_EQ((0), (std::strcmp((char *)hello, (char *)external.get())));
  // note part of the test is being sure that hello does not get deleted[] when external goes out of scope.
  // we count on the debug memory allocater to detect that case.
}

TEST(QuickFAST, TestBufferCollection)
{
  Communication::BufferCollection collection;
  EXPECT_TRUE(collection.pop() == 0);

  Communication::LinkedBuffer buffer1(20);
  buffer1[0] = 1;
  buffer1[1] = 1;
  buffer1.setUsed(2);

  Communication::LinkedBuffer buffer2(20);
  buffer2[0] = 2;
  buffer2[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer3(20);
  buffer2[0] = 3;
  buffer3[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer4(20);
  buffer4[0] = 4;
  buffer4[1] = 1;
  buffer4.setUsed(2);

  collection.push(&buffer1);

  EXPECT_TRUE(collection.pop() == &buffer1);
  EXPECT_TRUE(collection.pop() == 0);

  collection.push(&buffer1);
  collection.push(&buffer2);
  collection.push(&buffer3);
  collection.push(&buffer4);

  // notice we don't care what order we get them back
  // but we do want to see all of them exactly once each.
  Communication::LinkedBuffer * p = collection.pop();
  EXPECT_TRUE((*p)[1] == 1);
  (*p)[1] = 2;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 1);
  (*p)[1] = 2;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 1);
  (*p)[1] = 2;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 1);
  (*p)[1] = 2;

  p = collection.pop();
  EXPECT_TRUE(p == 0);

  EXPECT_TRUE(buffer1[1] == 2);
  EXPECT_TRUE(buffer2[1] == 2);
  EXPECT_TRUE(buffer3[1] == 2);
  EXPECT_TRUE(buffer4[1] == 2);

  Communication::BufferCollection collection2;
  collection2.push(&buffer4);
  collection2.push(&buffer3);
  collection2.push(&buffer2);
  collection2.push(&buffer1);
  collection.push(collection2);
  EXPECT_TRUE(collection2.pop() == 0);

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 2);
  (*p)[1] = 3;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 2);
  (*p)[1] = 3;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 2);
  (*p)[1] = 3;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 2);
  (*p)[1] = 3;

  p = collection.pop();
  EXPECT_TRUE(p == 0);

  EXPECT_TRUE(buffer1[1] == 3);
  EXPECT_TRUE(buffer2[1] == 3);
  EXPECT_TRUE(buffer3[1] == 3);
  EXPECT_TRUE(buffer4[1] == 3);

  collection.push(&buffer1);
  collection.push(&buffer2);
  collection2.push(&buffer3);
  collection2.push(&buffer4);
  collection.push(collection2);
  collection2.push(collection);
  collection.push(collection2);

  EXPECT_TRUE(collection2.pop() == 0);

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 3);
  (*p)[1] = 4;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 3);
  (*p)[1] = 4;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 3);
  (*p)[1] = 4;

  p = collection.pop();
  EXPECT_TRUE((*p)[1] == 3);
  (*p)[1] = 4;

  p = collection.pop();
  EXPECT_TRUE(p == 0);

  EXPECT_TRUE(buffer1[1] == 4);
  EXPECT_TRUE(buffer2[1] == 4);
  EXPECT_TRUE(buffer3[1] == 4);
  EXPECT_TRUE(buffer4[1] == 4);
}

TEST(QuickFAST, TestBufferQueue)
{
  Communication::BufferQueue queue1;
  Communication::BufferQueue queue2;
  Communication::LinkedBuffer buffer1(20);
  buffer1[0] = 1;
  buffer1[1] = 1;
  buffer1.setUsed(2);

  Communication::LinkedBuffer buffer2(20);
  buffer2[0] = 2;
  buffer2[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer3(20);
  buffer2[0] = 3;
  buffer3[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer4(20);
  buffer4[0] = 4;
  buffer4[1] = 1;
  buffer4.setUsed(2);

  // keep the first test simple
  // four buffers in, four buffers out
  // and make sure it works at least twice
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1));
    EXPECT_TRUE(!queue1.push(&buffer2));
    EXPECT_TRUE(queue1.pop() == &buffer1);
    EXPECT_TRUE(queue1.pop() == &buffer2);
    EXPECT_TRUE(queue1.pop() == 0);
  }

  // simple test of pushing a queue
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1));
    EXPECT_TRUE(!queue1.push(&buffer2));
    EXPECT_TRUE(queue2.push(queue1));
    EXPECT_TRUE(queue1.pop() == 0);
    EXPECT_TRUE(queue2.pop() == &buffer1);
    EXPECT_TRUE(queue2.pop() == &buffer2);
    EXPECT_TRUE(queue2.pop() == 0);
  }

  // test mixed arrivals and departures
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1));
    EXPECT_TRUE(!queue1.push(&buffer2));
    EXPECT_TRUE(!queue1.push(&buffer3));
    EXPECT_TRUE(queue1.pop() == &buffer1);
    EXPECT_TRUE(!queue1.push(&buffer4));
    EXPECT_TRUE(queue1.pop() == &buffer2);
    EXPECT_TRUE(!queue1.push(&buffer1));
    EXPECT_TRUE(queue1.pop() == &buffer3);
    EXPECT_TRUE(queue1.pop() == &buffer4);
    EXPECT_TRUE(queue1.pop() == &buffer1);
    EXPECT_TRUE(queue1.pop() == 0);
  }

  // test mixed arrivals and departures with 2 queues
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1));
    EXPECT_TRUE(!queue1.push(&buffer2));
    EXPECT_TRUE(queue2.push(queue1));
    EXPECT_TRUE(queue1.push(&buffer3));
    EXPECT_TRUE(queue2.pop() == &buffer1);
    EXPECT_TRUE(!queue2.push(queue1));
    EXPECT_TRUE(!queue2.push(queue1));
    EXPECT_TRUE(queue1.push(&buffer4));
    EXPECT_TRUE(queue2.pop() == &buffer2);
    EXPECT_TRUE(!queue1.push(&buffer1));
    EXPECT_TRUE(queue2.pop() == &buffer3);
    EXPECT_TRUE(queue2.pop() == 0);
    EXPECT_TRUE(queue2.push(queue1));
    EXPECT_TRUE(queue2.pop() == &buffer4);
    EXPECT_TRUE(queue2.pop() == &buffer1);
    EXPECT_TRUE(queue2.pop() == 0);
    EXPECT_TRUE(queue1.pop() == 0);
    EXPECT_TRUE(!queue2.push(queue1));
    EXPECT_TRUE(queue2.pop() == 0);
    EXPECT_TRUE(queue1.pop() == 0);
  }

}

TEST(QuickFAST, TestSingleServerBufferQueue)
{
  std::mutex dummyMutex;
  std::unique_lock<std::mutex> lock(dummyMutex);

  Communication::SingleServerBufferQueue queue1;

  Communication::LinkedBuffer buffer1(20);
  buffer1[0] = 1;
  buffer1[1] = 1;
  buffer1.setUsed(2);

  Communication::LinkedBuffer buffer2(20);
  buffer2[0] = 2;
  buffer2[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer3(20);
  buffer2[0] = 3;
  buffer3[1] = 1;
  buffer2.setUsed(2);

  Communication::LinkedBuffer buffer4(20);
  buffer4[0] = 4;
  buffer4[1] = 1;
  buffer4.setUsed(2);

  // keep the first test simple
  // four buffers in, four buffers out
  // and make sure it works at least twice
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1, lock));
    EXPECT_TRUE(queue1.push(&buffer2, lock));
    EXPECT_TRUE(queue1.startService(lock));
    EXPECT_TRUE(queue1.serviceNext() == &buffer1);
    EXPECT_TRUE(queue1.serviceNext() == &buffer2);
    EXPECT_TRUE(queue1.serviceNext() == 0);
    EXPECT_TRUE(!queue1.endService(true, lock));
  }

  // Next test gets more elaborate to simulate
  // buffers arriving while the queue is being serviced
  for(size_t loop = 0; loop < 2; ++loop)
  {
    EXPECT_TRUE(queue1.push(&buffer1, lock));
    EXPECT_TRUE(queue1.push(&buffer2, lock));
    EXPECT_TRUE(queue1.startService(lock));
    // be sure only one service at a time
    EXPECT_TRUE(!queue1.startService(lock));
    // push and return "service not necessary"
    EXPECT_TRUE(!queue1.push(&buffer3, lock));
    EXPECT_TRUE(queue1.serviceNext() == &buffer1);
    EXPECT_TRUE(!queue1.startService(lock));
    EXPECT_TRUE(!queue1.push(&buffer4, lock));
    EXPECT_TRUE(queue1.serviceNext() == &buffer2);
    EXPECT_TRUE(!queue1.push(&buffer1, lock));
    // batch should be complete even though new messages are queued
    EXPECT_TRUE(queue1.serviceNext() == 0);
    // so when we endService it should return true->there's more to do
    EXPECT_TRUE(queue1.endService(true, lock));
    EXPECT_TRUE(queue1.serviceNext() == &buffer3);
    EXPECT_TRUE(queue1.serviceNext() == &buffer4);
    EXPECT_TRUE(queue1.serviceNext() == &buffer1);
    EXPECT_TRUE(queue1.serviceNext() == 0);
    EXPECT_TRUE(!queue1.endService(true, lock));
  }
}

TEST(QuickFAST, TestStringBuffer)
{
  typedef StringBufferT<10> String10;
  const char * st1("12345");
  const char * st2("6789");

  String10 s1(st1);
  s1 += st2;
  EXPECT_TRUE(s1 == "123456789");
  EXPECT_TRUE(s1.growCount() == 0);
  s1 += 'A';
  EXPECT_TRUE(s1 == "123456789A");
  EXPECT_TRUE(s1.growCount() == 0);
  unsigned char b('B');
  s1 += b;
  EXPECT_TRUE(s1 == "123456789AB");
  EXPECT_TRUE(s1.growCount() == 1);

  String10 s2(s1);
  EXPECT_TRUE(s2 == "123456789AB");
  EXPECT_TRUE(s2.growCount() == 1);

  std::string stds2(s2);
  EXPECT_TRUE(stds2 == "123456789AB");
  size_t capacity = s2.capacity();
  size_t size = s2.size();
  EXPECT_TRUE(size <= capacity);
  size_t delta = capacity - size;
  String10 s3(delta, '!');
  EXPECT_TRUE(s3.size() == delta);
  s2 += s3;
  EXPECT_TRUE(s2.size() == capacity);
  EXPECT_TRUE(s2.growCount() == 1);
  s2 += "?";
  EXPECT_TRUE(s2.capacity() > capacity);
  EXPECT_TRUE(s2.growCount() == 2);
}

TEST(QuickFAST, TestWorkingBuffer)
{
  WorkingBuffer a;
  a.clear(false, 1);
  a.push('a');
  WorkingBuffer b;
  b.clear(false, 1);
  b.push('b');
  WorkingBuffer ab;
  ab.clear(false, a.size() + b.size());
  size_t abCapacity = ab.capacity();
  ab.append(a);
  ab.append(b);
  EXPECT_TRUE(ab.size() ==  a.size() + b.size());
  EXPECT_TRUE(ab.capacity() == abCapacity); // should not have forced a grow
  EXPECT_TRUE(0 == std::strncmp("ab", reinterpret_cast<const char *>(ab.begin()), ab.size()));

  WorkingBuffer ba;
  ba.clear(true, a.size() + b.size());
  size_t baCapacity = ab.capacity();
  ba.append(a);
  ba.append(b);
  EXPECT_TRUE(ba.size() ==  a.size() + b.size());
  EXPECT_TRUE(ba.capacity() == baCapacity); // should not have forced a grow
  EXPECT_TRUE(0 == std::strncmp("ba", reinterpret_cast<const char *>(ba.begin()), ba.size()));

  WorkingBuffer abc;
  size_t abcCap = abc.capacity();
  size_t abcHalf = abcCap / 2;
  std::string abcStr;
  abcStr.reserve(abcCap * 2);
  for(size_t n = 0; n < abcHalf; ++n)
  {
    abc.push(uchar('a' + n));
    abcStr += uchar('a' + n);
  }
  EXPECT_EQ((abc.size()), (abcHalf));
  EXPECT_EQ((abc.capacity()), (abcCap)); // no grow so far
  EXPECT_TRUE(0 == std::strncmp(abcStr.data(), reinterpret_cast<const char *>(abc.begin()), abcHalf));
  abc.append(abc); // also tests append-to-self
  abcStr += abcStr;
  EXPECT_TRUE(abc.size() == abcHalf * 2);
  EXPECT_TRUE(abc.capacity() == abcCap); // no grow so far
  EXPECT_TRUE(0 == std::strncmp(abcStr.data(), reinterpret_cast<const char *>(abc.begin()), abcHalf * 2));

  abc.append(abc); // This one should force a grow
  abcStr += abcStr;
  EXPECT_TRUE(abc.size() == abcHalf * 4);
  EXPECT_TRUE(abc.capacity() > abcCap); // now we should have grown
  EXPECT_TRUE(0 == std::strncmp(abcStr.data(), reinterpret_cast<const char *>(abc.begin()), abcHalf * 4));


  // TODO: This is a start, but we could use a lot more testing here.
}

TEST(QuickFAST, TestDecimal)
{
  // Test basic decimal arithmetic

  Decimal zero(0,0);
  Decimal one(1,0);
  Decimal ten(10,0);
  Decimal hundred(1,2);

  Decimal a(zero);
  a += one;
  EXPECT_EQ((a), (one));
  a += one;
  a += one;
  a += one;
  a += one;
  a += one;
  a += one;
  a += one;
  a += one;
  a += one;
  EXPECT_EQ((a), (ten));
  a /= ten;
  a -= one;
  EXPECT_EQ((a), (zero));
  a = hundred;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  a -= ten;
  EXPECT_EQ((a), (zero));

  a = one;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a /= ten;
  a *= hundred;
  a *= hundred;
  a *= hundred;
  a *= hundred;
  a *= hundred;
  EXPECT_EQ((a), (one));

  Decimal b(0,0);
  Decimal c(123,0);
  Decimal d(456,0);
  b += c;
  b += d;
  b -= d;
  EXPECT_EQ((b), (c));

  Decimal f(2095, -2);
  Decimal g(2000, -2);
  EXPECT_GT(f, g);
  f.normalize();
  g.normalize();
  EXPECT_GT(f, g);

}
