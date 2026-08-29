// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <string>

#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Communication/PCapReader.h>

#include "TestPaths.h"

using namespace QuickFAST;

namespace
{
  std::string capture(const char * name)
  {
    return TestPaths::resource((std::string("/src/Tests/resources/pcap/") + name).c_str());
  }

  std::string templateFile()
  {
    return TestPaths::resource("/src/Tests/resources/unittest_mandatory.xml");
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
  EXPECT_THROW(configureFromCapture("modern.pcapng"), UsageError);
  EXPECT_THROW(configureFromCapture("not-a-capture.bin"), UsageError);
  EXPECT_THROW(configureFromCapture("no-such-file.pcap"), UsageError);
}

namespace
{
  const char payload[] = "QUICKFAST-PCAP-CORPUS";

  // set32bit selects the only packet-record layout classic pcap actually has;
  // the default is finding #17 and is fixed separately.
  void openCapture(Communication::PCapReader & reader, const char * name)
  {
    reader.set32bit(true);
    ASSERT_TRUE(reader.open(capture(name).c_str()));
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

TEST(QuickFAST, testPCapReaderReadsAWellFormedCapture)
{
  Communication::PCapReader reader;
  openCapture(reader, "classic-le.pcap");

  const unsigned char * buffer = 0;
  size_t size = 0;
  ASSERT_TRUE(reader.read(buffer, size));
  ASSERT_EQ((size), (sizeof(payload) - 1));
  EXPECT_EQ((std::string(reinterpret_cast<const char *>(buffer), size)), (std::string(payload)));

  // One packet in the file, so the next read is a clean end of data.
  EXPECT_FALSE(reader.read(buffer, size));
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
    EXPECT_FALSE(reader.open(TestPaths::resource("/src/Tests/resources").c_str()));
  });
  EXPECT_FALSE(reader.good());
}
