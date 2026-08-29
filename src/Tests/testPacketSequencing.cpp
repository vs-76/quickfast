// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// Five things about PacketSequencingAssembler.
//
// serviceQueue declared "result" as true and returned it unchanged, while the
// answer it should have returned was being discarded one level down in
// processPacket. So setMessageLimit was inoperative and a builder asking to
// stop was ignored.
//
// lookAheadCount is a constructor parameter and zero is a reasonable thing to
// pass -- sequence checking without look-ahead buffering -- but "new
// LinkedBuffer *[0]" succeeds and the first line of the service loop then
// divides by it.
//
// The sequence number was read without reference to how many bytes arrived, so
// a datagram too short to carry one got a number assembled from stale bytes
// left in the pooled buffer by an earlier packet.
//
// findGapEnd stated its precondition in a comment and then dereferenced the
// pointer it was about.
//
// And a loss at the 32-bit wraparound boundary turned every subsequent packet
// into a stale one, silently and permanently.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Codecs/PacketSequencingAssembler.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>
#include <Codecs/NoHeaderAnalyzer.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Communication/SynchReceiver.h>
#include <Messages/ValueMessageBuilder.h>
#include <Common/Exceptions.h>

using namespace QuickFAST;

namespace
{
  /// @brief A builder that records what it is told and can ask to stop.
  class RecordingBuilder : public Messages::ValueMessageBuilder
  {
  public:
    std::vector<std::string> errors_;
    std::vector<std::pair<uint32, uint32> > gaps_;
    size_t messages_ = 0;
    bool keepGoing_ = true;

    virtual const std::string & getApplicationType() const
    {
      static const std::string type("recording");
      return type;
    }
    virtual const std::string & getApplicationTypeNs() const
    {
      static const std::string ns;
      return ns;
    }
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int64) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint64) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int32) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint32) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int16) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint16) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const int8) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const uchar) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const Decimal &) {}
    virtual void addValue(const Messages::FieldIdentity &, ValueType::Type, const unsigned char *, size_t) {}
    virtual ValueMessageBuilder & startMessage(const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual bool endMessage(ValueMessageBuilder &)
    {
      ++messages_;
      return true;
    }
    virtual bool ignoreMessage(ValueMessageBuilder &)
    {
      return true;
    }
    virtual ValueMessageBuilder & startSequence(
      const Messages::FieldIdentity &, const std::string &, const std::string &,
      size_t, const Messages::FieldIdentity &, size_t)
    {
      return *this;
    }
    virtual void endSequence(const Messages::FieldIdentity &, ValueMessageBuilder &) {}
    virtual ValueMessageBuilder & startSequenceEntry(const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual void endSequenceEntry(ValueMessageBuilder &) {}
    virtual ValueMessageBuilder & startGroup(
      const Messages::FieldIdentity &, const std::string &, const std::string &, size_t)
    {
      return *this;
    }
    virtual void endGroup(const Messages::FieldIdentity &, ValueMessageBuilder &) {}

    virtual void reportGap(uint32 from, uint32 to)
    {
      gaps_.push_back(std::make_pair(from, to));
    }

    virtual bool wantLog(unsigned short)
    {
      return false;
    }
    virtual bool logMessage(unsigned short, const std::string &)
    {
      return true;
    }
    virtual bool reportDecodingError(const std::string & errorMessage)
    {
      errors_.push_back(errorMessage);
      return keepGoing_;
    }
    virtual bool reportCommunicationError(const std::string & errorMessage)
    {
      errors_.push_back(errorMessage);
      return keepGoing_;
    }
  };

  Codecs::TemplateRegistryPtr trivialRegistry()
  {
    std::stringstream templates(
      "<templates>"
      "  <template name=\"t\" id=\"1\">"
      "    <uInt32 name=\"value\"/>"
      "  </template>"
      "</templates>");
    Codecs::XMLTemplateParser parser;
    return parser.parse(templates);
  }

  /// @brief A packet: a four byte big-endian sequence number then one message.
  std::string packet(uint32 sequenceNumber)
  {
    std::string result;
    result.push_back(char((sequenceNumber >> 24) & 0xFF));
    result.push_back(char((sequenceNumber >> 16) & 0xFF));
    result.push_back(char((sequenceNumber >> 8) & 0xFF));
    result.push_back(char(sequenceNumber & 0xFF));
    result.append("\xC0\x81\x82", 3);
    return result;
  }

  /// @brief A receiver that delivers a fixed list of datagrams and stops.
  ///
  /// One buffer per packet, because that is what a datagram receiver does and
  /// what the sequencing assembler assumes: it reads one sequence number from
  /// the front of each buffer. Delivering the whole list within a single
  /// receiver session is what lets a test observe the assembler asking to
  /// stop, which is the thing findings #64 is about.
  class PacketListReceiver : public Communication::SynchReceiver
  {
  public:
    explicit PacketListReceiver(const std::vector<std::string> & packets)
      : packets_(packets)
    {
    }

  private:
    virtual bool initializeReceiver()
    {
      return true;
    }

    virtual bool fillBuffer(
      Communication::LinkedBuffer * buffer,
      std::unique_lock<std::mutex> & lock)
    {
      if(next_ >= packets_.size())
      {
        return false;
      }
      const std::string & packet = packets_[next_++];
      std::memcpy(buffer->get(), packet.data(), packet.size());
      acceptFullBuffer(buffer, packet.size(), lock);
      return true;
    }

    virtual void resetService()
    {
    }

    const std::vector<std::string> & packets_;
    size_t next_ = 0;
  };

  /// @brief Feed packets through a sequencing assembler, one datagram each.
  void feed(
    Codecs::PacketSequencingAssembler & assembler,
    const std::vector<std::string> & packets)
  {
    PacketListReceiver receiver(packets);
    // The pool is exactly as large as the list, so start() fills every buffer
    // and leaves the queue to be serviced rather than running dry first.
    receiver.start(assembler, 1500, packets.size());
    receiver.runThreads(1, true);
  }
}

/// @brief A zero look-ahead must be refused, not divided by.
TEST(QuickFAST, testZeroLookAheadIsRejected)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;

  EXPECT_THROW(
    Codecs::PacketSequencingAssembler(
      trivialRegistry(), packetHeader, messageHeader, builder, 0,
      Communication::RecoveryFeedPtr()),
    UsageError);
  EXPECT_NO_THROW(
    Codecs::PacketSequencingAssembler(
      trivialRegistry(), packetHeader, messageHeader, builder, 1,
      Communication::RecoveryFeedPtr()));
}

/// @brief A packet too short to carry a sequence number must be reported.
TEST(QuickFAST, testShortPacketIsNotRoutedOnStaleBytes)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::PacketSequencingAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder, 4,
    Communication::RecoveryFeedPtr());

  // Two bytes: not enough for the four byte sequence number.
  feed(assembler, {std::string("\x00\x01", 2)});

  ASSERT_FALSE(builder.errors_.empty());
  EXPECT_NE(std::string::npos, builder.errors_.front().find("Sequence number"));
  EXPECT_EQ(0u, builder.messages_);
}

/// @brief The message limit must actually stop the assembler.
TEST(QuickFAST, testMessageLimitStopsTheSequencingAssembler)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::PacketSequencingAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder, 4,
    Communication::RecoveryFeedPtr());
  assembler.setMessageLimit(1);

  feed(assembler, {packet(0), packet(1), packet(2)});

  EXPECT_LE(builder.messages_, 2u)
    << "the message limit was computed, returned, and dropped";
}

/// @brief A builder asking to stop must be obeyed.
TEST(QuickFAST, testBuilderStopRequestIsObeyed)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  builder.keepGoing_ = false;
  Codecs::PacketSequencingAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder, 4,
    Communication::RecoveryFeedPtr());

  // Two bytes: too short, so the builder is asked and says stop.
  feed(assembler, {std::string("\x00\x01", 2)});

  EXPECT_EQ(1u, builder.errors_.size())
    << "the assembler kept going after being told to stop";
}

/// @brief In-order packets still decode.
TEST(QuickFAST, testSequencedPacketsInOrderStillDecode)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::PacketSequencingAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder, 4,
    Communication::RecoveryFeedPtr());

  feed(assembler, {packet(7), packet(8), packet(9)});

  EXPECT_TRUE(builder.errors_.empty())
    << (builder.errors_.empty() ? "" : builder.errors_.front());
  EXPECT_EQ(3u, builder.messages_);
  EXPECT_TRUE(builder.gaps_.empty());
}

/// @brief A loss at the wraparound boundary must be a gap, not a dead feed.
///
/// With the next expected number at 0xFFFFFFFE and packet 1 arriving,
/// "1 < 0xFFFFFFFE" is true under plain comparison, so the packet was released
/// as stale. Every packet after it went the same way, and the gap was never
/// reported because gap detection needs packets to accumulate in the deferred
/// queue.
TEST(QuickFAST, testLossAtTheWraparoundBoundaryIsAGap)
{
  Codecs::FixedSizeHeaderAnalyzer packetHeader(0, true, 4, 0, 0, 4);
  Codecs::NoHeaderAnalyzer messageHeader;
  RecordingBuilder builder;
  Codecs::PacketSequencingAssembler assembler(
    trivialRegistry(), packetHeader, messageHeader, builder, 4,
    Communication::RecoveryFeedPtr());

  // 0xFFFFFFFE arrives, 0xFFFFFFFF is lost, then 0 and 1 arrive.
  feed(assembler, {packet(0xFFFFFFFE), packet(0), packet(1)});

  EXPECT_EQ(3u, builder.messages_)
    << "packets after the wrap were discarded as stale";
  ASSERT_EQ(1u, builder.gaps_.size());
  EXPECT_EQ(0xFFFFFFFFu, builder.gaps_.front().first);
  EXPECT_EQ(0u, builder.gaps_.front().second);
}
