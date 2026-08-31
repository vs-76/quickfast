// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// ASCII and byteVector decode must agree for contiguous buffers, one-byte
// sources, and mid-string / mid-vector splits.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/Encoder.h>
#include <Codecs/Decoder.h>
#include <Codecs/DataDestination.h>
#include <Codecs/DataSource.h>
#include <Codecs/DataSourceBuffer.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/FieldInstruction.h>
#include <Common/WorkingBuffer.h>
#include <Common/Exceptions.h>

#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldByteVector.h>

#include <string>
#include <algorithm>

using namespace QuickFAST;

namespace
{
  class OneByteSource : public Codecs::DataSource
  {
  public:
    explicit OneByteSource(std::string bytes)
      : storage_(std::move(bytes))
      , next_(0)
    {
    }

    bool getBuffer(const uchar *& buffer, size_t & size) override
    {
      if(next_ >= storage_.size())
      {
        return false;
      }
      buffer = reinterpret_cast<const uchar *>(storage_.data()) + next_;
      size = 1;
      ++next_;
      return true;
    }

  private:
    std::string storage_;
    size_t next_;
  };

  class SplitSource : public Codecs::DataSource
  {
  public:
    SplitSource(std::string bytes, size_t splitAt)
      : storage_(std::move(bytes))
      , splitAt_(std::min(splitAt, storage_.size()))
      , stage_(0)
    {
    }

    bool getBuffer(const uchar *& buffer, size_t & size) override
    {
      while(stage_ < 2)
      {
        if(stage_ == 0)
        {
          buffer = reinterpret_cast<const uchar *>(storage_.data());
          size = splitAt_;
          ++stage_;
          if(size > 0)
          {
            return true;
          }
          continue;
        }
        buffer = reinterpret_cast<const uchar *>(storage_.data()) + splitAt_;
        size = storage_.size() - splitAt_;
        ++stage_;
        return size > 0;
      }
      return false;
    }

  private:
    std::string storage_;
    size_t splitAt_;
    int stage_;
  };

  const Messages::FieldIdentity asciiId("s");
  const Messages::FieldIdentity blobId("b");

  Codecs::TemplateRegistryPtr asciiRegistry()
  {
    std::stringstream xml(
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <string name=\"s\" charset=\"ascii\"><nop/></string>"
      "  </template>"
      "</templates>");
    return Codecs::XMLTemplateParser().parse(xml);
  }

  Codecs::TemplateRegistryPtr blobRegistry()
  {
    std::stringstream xml(
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <byteVector name=\"b\"><nop/></byteVector>"
      "  </template>"
      "</templates>");
    return Codecs::XMLTemplateParser().parse(xml);
  }

  std::string encodeAscii(const std::string & value)
  {
    auto registry = asciiRegistry();
    Messages::Message message(registry->maxFieldCount());
    message.addField(asciiId, Messages::FieldAscii::create(value));
    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, 1, message);
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  std::string encodeBlob(const std::string & value)
  {
    auto registry = blobRegistry();
    Messages::Message message(registry->maxFieldCount());
    message.addField(blobId, Messages::FieldByteVector::create(value));
    Codecs::Encoder encoder(registry);
    Codecs::DataDestination destination;
    encoder.encodeMessage(destination, 1, message);
    std::string fast;
    destination.toString(fast);
    return fast;
  }

  std::string decodeAsciiFrom(Codecs::DataSource & source)
  {
    Codecs::Decoder decoder(asciiRegistry());
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    decoder.decodeMessage(source, builder);
    Messages::FieldCPtr field;
    EXPECT_TRUE(consumer.message().getField("s", field));
    return std::string(
      reinterpret_cast<const char *>(field->toAscii().data()),
      field->toAscii().size());
  }

  std::string decodeBlobFrom(Codecs::DataSource & source)
  {
    Codecs::Decoder decoder(blobRegistry());
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    decoder.decodeMessage(source, builder);
    Messages::FieldCPtr field;
    EXPECT_TRUE(consumer.message().getField("b", field));
    return std::string(
      reinterpret_cast<const char *>(field->toByteVector().data()),
      field->toByteVector().size());
  }
}

TEST(QuickFAST, testAsciiDecodeContiguousMatchesOneByte)
{
  const std::string values[] = {
    "",
    "a",
    "hello",
    std::string(80, 'z'),
  };
  for(const std::string & value : values)
  {
    SCOPED_TRACE(value.size());
    const std::string fast = encodeAscii(value);
    Codecs::DataSourceBuffer contiguous(
      reinterpret_cast<const unsigned char *>(fast.data()), fast.size());
    OneByteSource oneByte(fast);
    EXPECT_EQ(value, decodeAsciiFrom(contiguous));
    EXPECT_EQ(value, decodeAsciiFrom(oneByte));
  }
}

TEST(QuickFAST, testAsciiDecodeSurvivesSplitBeforeStopBit)
{
  const std::string value(40, 'q');
  const std::string fast = encodeAscii(value);
  ASSERT_GT(fast.size(), 2u);
  for(size_t split = 1; split < fast.size(); ++split)
  {
    SCOPED_TRACE(split);
    SplitSource source(fast, split);
    EXPECT_EQ(value, decodeAsciiFrom(source));
  }
}

TEST(QuickFAST, testByteVectorDecodeContiguousMatchesOneByte)
{
  const std::string values[] = {
    "",
    std::string("\0\x01\x02", 3),
    std::string(100, '\xAB'),
  };
  for(const std::string & value : values)
  {
    SCOPED_TRACE(value.size());
    const std::string fast = encodeBlob(value);
    Codecs::DataSourceBuffer contiguous(
      reinterpret_cast<const unsigned char *>(fast.data()), fast.size());
    OneByteSource oneByte(fast);
    EXPECT_EQ(value, decodeBlobFrom(contiguous));
    EXPECT_EQ(value, decodeBlobFrom(oneByte));
  }
}

TEST(QuickFAST, testByteVectorDecodeSurvivesSplit)
{
  const std::string value(50, 'B');
  const std::string fast = encodeBlob(value);
  ASSERT_GT(fast.size(), 2u);
  for(size_t split = 1; split < fast.size(); ++split)
  {
    SCOPED_TRACE(split);
    SplitSource source(fast, split);
    EXPECT_EQ(value, decodeBlobFrom(source));
  }
}

TEST(QuickFAST, testByteVectorRejectsOversizeLength)
{
  // Length-prefixed oversize: craft a tiny template decode with a huge length
  // via FieldInstruction::decodeByteVector directly.
  Codecs::Decoder decoder(blobRegistry());
  decoder.setMaxByteVectorLength(8);
  WorkingBuffer buffer;
  OneByteSource source(std::string(16, 'x'));
  EXPECT_THROW(
    Codecs::FieldInstruction::decodeByteVector(
      decoder, source, "b", buffer, 64),
    EncodingError);
}

TEST(QuickFAST, testByteVectorTruncatedInputIsFatal)
{
  Codecs::Decoder decoder(blobRegistry());
  WorkingBuffer buffer;
  OneByteSource source(std::string(2, 'x'));
  EXPECT_THROW(
    Codecs::FieldInstruction::decodeByteVector(
      decoder, source, "b", buffer, 10),
    EncodingError);
}
