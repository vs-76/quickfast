// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// startReceive reads until a read fails, and for a synchronous source a failed
// read is end of file. It then called stop(), which every run loop reads as
// "stop working" rather than "stop reading". With the default two buffer pool
// a one packet file is fully consumed inside Receiver::start: the packet is
// queued, the next read hits EOF, stopping_ is set, and the caller's event loop
// never services the packet that is sitting right there.
//
// Three packets survive only because the pool runs dry before EOF is reached.
// That is the whole of the bug: it depends on the arithmetic between packet
// count and buffer count, not on anything about the data.
#include <Common/QuickFASTPch.h>
#include <gtest/gtest.h>

#include <Communication/RawFileReceiver.h>
#include <Communication/BufferedRawFileReceiver.h>
#include <Communication/Assembler.h>
#include <Codecs/TemplateRegistry.h>
#include <Common/Logger.h>
#include "TempFile.h"

using namespace QuickFAST;

namespace
{
  /// @brief A logger that says nothing, because nothing is expected.
  class SilentLogger : public Common::Logger
  {
  public:
    virtual bool wantLog(unsigned short)
    {
      return false;
    }
    virtual bool logMessage(unsigned short, const std::string &)
    {
      return true;
    }
    virtual bool reportDecodingError(const std::string &)
    {
      return true;
    }
    virtual bool reportCommunicationError(const std::string &)
    {
      return true;
    }
  };

  /// @brief Counts buffers rather than decoding them.
  class CountingAssembler : public Communication::Assembler
  {
  public:
    CountingAssembler(Common::Logger & logger)
      : Communication::Assembler(
          Codecs::TemplateRegistryPtr(new Codecs::TemplateRegistry), logger)
    {
    }

    size_t buffers_ = 0;
    size_t bytes_ = 0;

    virtual bool serviceQueue(Communication::Receiver & receiver)
    {
      Communication::LinkedBuffer * buffer = receiver.getBuffer(false);
      while(buffer != 0)
      {
        ++buffers_;
        bytes_ += buffer->used();
        receiver.releaseBuffer(buffer);
        buffer = receiver.getBuffer(false);
      }
      return false;
    }

    virtual void receiverStarted(Communication::Receiver &) {}
    virtual void receiverStopped(Communication::Receiver &) {}
  };

  using TestPaths::TemporaryFile;

  /// @brief Read a whole file through a raw receiver and report what arrived.
  size_t bytesDelivered(const std::string & fileName, size_t bufferSize, size_t bufferCount)
  {
    SilentLogger logger;
    CountingAssembler assembler(logger);
    std::ifstream stream(fileName.c_str(), std::ios::binary);
    Communication::RawFileReceiver receiver(stream);
    receiver.start(assembler, bufferSize, bufferCount);
    receiver.runThreads(1, true);
    receiver.joinThreads();
    return assembler.bytes_;
  }
}

/// @brief A file smaller than the buffer pool must still be delivered.
///
/// One read fills the only packet, the second hits EOF, and with two buffers
/// both happen inside start(). Before the fix the queued packet was discarded.
TEST(QuickFAST, testShortFileIsDeliveredNotDiscarded)
{
  const std::string contents("hello");
  TemporaryFile file(contents);
  EXPECT_EQ(contents.size(), bytesDelivered(file.name(), 1500, 2));
}

/// @brief The same file with a single buffer, which never had the problem.
///
/// One buffer means the pool runs dry before EOF is reached, so startReceive
/// exits without a failed read. Pinning it makes clear the defect was in the
/// arithmetic between packet count and buffer count.
TEST(QuickFAST, testShortFileWithOneBufferStillWorks)
{
  const std::string contents("hello");
  TemporaryFile file(contents);
  EXPECT_EQ(contents.size(), bytesDelivered(file.name(), 1500, 1));
}

/// @brief Every buffer count must deliver the whole file, not just some.
TEST(QuickFAST, testWholeFileArrivesForEveryBufferCount)
{
  std::string contents;
  for(int i = 0; i < 32; ++i)
  {
    contents.append("0123456789abcdef");
  }
  TemporaryFile file(contents);

  // A buffer size of 64 means eight reads, so the pool runs dry for small
  // counts and hits EOF first for large ones. Both must deliver everything.
  for(size_t bufferCount = 1; bufferCount <= 16; ++bufferCount)
  {
    EXPECT_EQ(contents.size(), bytesDelivered(file.name(), 64, bufferCount))
      << "bufferCount=" << bufferCount;
  }
}

/// @brief An empty file must deliver nothing and must not hang.
TEST(QuickFAST, testEmptyFileTerminates)
{
  TemporaryFile file("");
  EXPECT_EQ(0u, bytesDelivered(file.name(), 1500, 2));
}

/// @brief The buffered raw receiver has the same shape and the same bug.
TEST(QuickFAST, testShortFileIsDeliveredByTheBufferedReceiver)
{
  const std::string contents("hello");
  TemporaryFile file(contents);

  SilentLogger logger;
  CountingAssembler assembler(logger);
  std::ifstream stream(file.name().c_str(), std::ios::binary);
  Communication::BufferedRawFileReceiver receiver(stream);
  receiver.start(assembler, 1500, 2);
  receiver.runThreads(1, true);
  receiver.joinThreads();
  EXPECT_EQ(contents.size(), assembler.bytes_);
}
