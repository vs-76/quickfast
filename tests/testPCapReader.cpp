// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Communication/PCapFileReceiver.h>
#include <Communication/PCapReader.h>
#include <Codecs/TemplateRegistry.h>

#include "TestPaths.h"

using namespace QuickFAST;

namespace
{
  std::string capture(const char * name)
  {
    return TestPaths::resource((std::string("/tests/resources/pcap/") + name).c_str());
  }

  std::string templateFile()
  {
    return TestPaths::resource("/tests/resources/unittest_mandatory.xml");
  }

  void configureFromCapture(const char * captureName)
  {
    Application::DecoderConfiguration configuration;
    configuration.setTemplateFileName(templateFile());
    configuration.setReceiverType(Application::DecoderConfiguration::PCAPFILE_RECEIVER);
    configuration.setPcapFileName(capture(captureName));

    // Nothing is expected to decode: these tests are about the capture-file
    // layer, so the consumer only has to exist.
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    Application::DecoderConnection connection;
    connection.configure(builder, configuration);
  }
}

TEST(QuickFAST, testUnreadableCaptureIsFatalRatherThanASilentStall)
{
  // Receiver::start() returns false when the capture cannot be opened, but
  // DecoderConnection discarded that result: the caller then ran an event loop
  // over a receiver that had never been started and no buffers allocated, so
  // the process reported the problem and waited forever for data that could
  // never arrive.  Refusing to configure turns that hang into a clean exit.
  EXPECT_THROW(configureFromCapture("not-a-capture.bin"), UsageError);
  EXPECT_THROW(configureFromCapture("no-such-file.pcap"), UsageError);
}

namespace
{
  const char payload[] = "QUICKFAST-PCAP-CORPUS";

  class SilentLogger : public Common::Logger
  {
  public:
    virtual bool wantLog(unsigned short) { return false; }
    virtual bool logMessage(unsigned short, const std::string &) { return true; }
    virtual bool reportDecodingError(const std::string &) { return true; }
    virtual bool reportCommunicationError(const std::string &) { return true; }
  };

  // Collects the bytes a Receiver delivers, so the test can assert on what a
  // real consumer would see rather than only on the reader's out-parameters.
  class CapturingAssembler : public Communication::Assembler
  {
  public:
    CapturingAssembler()
      : Communication::Assembler(
          Codecs::TemplateRegistryPtr(new Codecs::TemplateRegistry(1, 1, 1)),
          logger_)
    {
    }

    virtual void receiverStarted(Communication::Receiver &) {}
    virtual void receiverStopped(Communication::Receiver &) {}

    virtual bool serviceQueue(Communication::Receiver & receiver)
    {
      Communication::LinkedBuffer * buffer = 0;
      while((buffer = receiver.getBuffer(false)) != 0)
      {
        packets.push_back(
          std::string(reinterpret_cast<const char *>(buffer->get()), buffer->used()));
        receiver.releaseBuffer(buffer);
      }
      return true;
    }

    std::vector<std::string> packets;

  private:
    SilentLogger logger_;
  };

  void openCapture(Communication::PCapReader & reader, const char * name)
  {
    ASSERT_TRUE(reader.open(capture(name).c_str())) << name << ": " << reader.errorMessage();
  }

  // Read the one datagram a well-formed corpus file carries.
  void expectSinglePayloadPacket(const char * name)
  {
    Communication::PCapReader reader;
    openCapture(reader, name);

    const unsigned char * buffer = 0;
    size_t size = 0;
    ASSERT_TRUE(reader.read(buffer, size)) << name << ": " << reader.errorMessage();
    ASSERT_EQ((size), (sizeof(payload) - 1)) << name;
    EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer), size)), (std::string(payload)))
      << name;

    // One packet per file, so the next read is a clean end of data.
    EXPECT_FALSE(reader.read(buffer, size)) << name;
    EXPECT_TRUE(reader.atEnd()) << name;
    EXPECT_EQ((reader.errorMessage()), (std::string())) << name;
  }

  // Drain a capture, asserting only that nothing is reported out of bounds.
  // Every length in these files is attacker-controlled, so the contract is
  // that a returned buffer lies inside the capture and a returned size does
  // not run past its end -- not that any particular packet is accepted.
  void expectNoOutOfBoundsPackets(const char * name)
  {
    Communication::PCapReader reader;
    openCapture(reader, name);

    const unsigned char * buffer = 0;
    size_t size = 0;
    size_t packets = 0;
    while(reader.read(buffer, size) && packets < 100)
    {
      ++packets;
      ASSERT_TRUE(buffer != 0) << name;
      // A cargo size that outgrew the whole capture is the underflow
      // signature: size_t wrapping turns "udplen - 8" into ~2^64.
      EXPECT_LT((size), (size_t(1) << 20)) << name << ": implausible cargo size";
      // Touch every returned byte so a sanitizer build sees the read the
      // caller would perform.
      volatile unsigned char sink = 0;
      for(size_t pos = 0; pos < size; ++pos)
      {
        sink = static_cast<unsigned char>(sink ^ buffer[pos]);
      }
      (void)sink;
    }
  }
}

TEST(QuickFAST, testPCapReaderReadsEveryCaptureFormat)
{
  // The default configuration used to read the record header through a
  // 24 byte struct timeval layout that no savefile has ever used, so caplen
  // and len came out of payload bytes, differed, and every packet in a valid
  // capture was discarded as truncated while the process exited 0.
  expectSinglePayloadPacket("classic-le.pcap");
  expectSinglePayloadPacket("classic-be.pcap");

  // Both of these were reported as "Invalid pcap file: missing magic." even
  // though each carries a perfectly valid magic.
  expectSinglePayloadPacket("nanosecond.pcap");
  expectSinglePayloadPacket("modern.pcapng");
}

TEST(QuickFAST, testPCapReaderRewindsToTheFirstPacket)
{
  // A savefile is a forward-only stream, so replaying it means reopening.
  Communication::PCapReader reader;
  openCapture(reader, "modern.pcapng");

  const unsigned char * buffer = 0;
  size_t size = 0;
  ASSERT_TRUE(reader.read(buffer, size));
  const std::string first(reinterpret_cast<const char *>(buffer), size);
  ASSERT_FALSE(reader.read(buffer, size));

  ASSERT_TRUE(reader.rewind());
  ASSERT_TRUE(reader.good());
  ASSERT_TRUE(reader.read(buffer, size));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer), size)), (first));
}

TEST(QuickFAST, testPCapReaderReportsWhyAFileWasRejected)
{
  Communication::PCapReader reader;
  EXPECT_FALSE(reader.open(capture("not-a-capture.bin").c_str()));
  EXPECT_FALSE(reader.good());
  // The old reader blamed a missing magic whatever the real problem was.
  EXPECT_FALSE(reader.errorMessage().empty());
  EXPECT_FALSE(reader.atEnd());

  EXPECT_FALSE(reader.open(capture("no-such-file.pcap").c_str()));
  EXPECT_FALSE(reader.errorMessage().empty());
}

TEST(QuickFAST, testPCapReaderRejectsMalformedWireLengths)
{
  // Each of these carries a length field a capture is free to lie about, and
  // each was subtracted from an unsigned counter with no check first.
  expectNoOutOfBoundsPackets("short-link-header.pcap");
  expectNoOutOfBoundsPackets("udp-length-zero.pcap");
  expectNoOutOfBoundsPackets("udp-length-overlong.pcap");
  expectNoOutOfBoundsPackets("ip-header-overlong.pcap");
  expectNoOutOfBoundsPackets("ip-header-undersized.pcap");
  expectNoOutOfBoundsPackets("caplen-beyond-file.pcap");
  expectNoOutOfBoundsPackets("truncated-record.pcap");
}

TEST(QuickFAST, testPCapReaderHandlesACaptureWithNoPackets)
{
  Communication::PCapReader reader;
  openCapture(reader, "empty.pcap");
  const unsigned char * buffer = 0;
  size_t size = 0;
  EXPECT_FALSE(reader.read(buffer, size));
}

TEST(QuickFAST, testPCapReaderRejectsAFileThatCannotBeSized)
{
  // ftell returns -1L for a directory, which became SIZE_MAX in a size_t and
  // then a new[] of that many bytes -- a length_error escaping a function
  // whose contract is to report failure by returning false.
  Communication::PCapReader reader;
  EXPECT_NO_THROW({
    EXPECT_FALSE(reader.open(TestPaths::resource("/tests/resources").c_str()));
  });
  EXPECT_FALSE(reader.good());
}

TEST(QuickFAST, testPCapFileReceiverDeliversTheCargo)
{
  // The whole point of the reader: the bytes that reach a Receiver's buffer
  // are the UDP cargo, with every layer of header removed.
  Communication::PCapFileReceiver receiver(capture("three-packets.pcap"));
  CapturingAssembler assembler;
  ASSERT_TRUE(receiver.start(assembler));
  receiver.poll();

  EXPECT_EQ((assembler.packets.size()), (3u));
  for(size_t n = 0; n < assembler.packets.size(); ++n)
  {
    EXPECT_EQ((assembler.packets[n]), (std::string(payload)));
  }
}

TEST(QuickFAST, testPCapFileReceiverDeliversAOnePacketCapture)
{
  // Fewer packets than buffers means the file is read to EOF inside start(),
  // and end of file used to be indistinguishable from a stop request: the one
  // packet sat in the queue and the run loop never looked at it. Both capture
  // formats, because the defect is in the Receiver and neither reader knows
  // anything about it.
  for(const char * name : {"classic-le.pcap", "modern.pcapng"})
  {
    Communication::PCapFileReceiver receiver(capture(name));
    CapturingAssembler assembler;
    ASSERT_TRUE(receiver.start(assembler, 1500, 2)) << name;
    receiver.poll();

    ASSERT_EQ(1u, assembler.packets.size()) << name;
    EXPECT_EQ(std::string(payload), assembler.packets.front()) << name;
  }
}
