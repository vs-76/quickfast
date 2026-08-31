// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// stop(), pause() and resume() are public, take no lock, and are called from
// whatever thread drives shutdown, while stopping_ is read on the service
// thread and on asio completion threads. As plain bools that is a data race,
// so this test drives a receiver on one thread and stops it from another,
// which ThreadSanitizer reports against the unfixed code and passes silently
// against the fixed code. Without a sanitizer it is still worth having: it
// asserts the receiver actually notices the stop.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Communication/Receiver.h>
#include <Communication/SynchReceiver.h>
#include <Communication/Assembler.h>
#include <Codecs/TemplateRegistry.h>
#include <Common/Logger.h>

#include <chrono>
#include <thread>

using namespace QuickFAST;

namespace
{
  /// @brief A receiver that never runs out of data.
  ///
  /// The point is to keep the service thread inside the loops that read
  /// stopping_ for long enough that a stop from another thread lands in the
  /// middle of one.
  class EndlessReceiver : public Communication::SynchReceiver
  {
  private:
    virtual bool initializeReceiver()
    {
      return true;
    }

    virtual bool fillBuffer(
      Communication::LinkedBuffer * buffer,
      std::unique_lock<std::mutex> & lock)
    {
      // Bounded, because getBuffer refills from within its own loop: a source
      // that never runs dry keeps the service thread inside a single
      // serviceQueue call forever.
      if(stopping_ || ++filled_ > 200000)
      {
        return false;
      }
      std::memset(buffer->get(), 0, buffer->capacity());
      acceptFullBuffer(buffer, buffer->capacity(), lock);
      return true;
    }

    virtual void resetService()
    {
    }

    size_t filled_ = 0;
  };

  /// @brief A logger that throws everything away.
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

  /// @brief An assembler that consumes whatever it is given.
  class CountingAssembler : public Communication::Assembler
  {
  public:
    CountingAssembler(Codecs::TemplateRegistryPtr registry, Common::Logger & logger)
      : Communication::Assembler(registry, logger)
    {
    }

    std::atomic<size_t> services_{0};

    virtual void receiverStarted(Communication::Receiver &)
    {
    }
    virtual void receiverStopped(Communication::Receiver &)
    {
    }
    virtual bool serviceQueue(Communication::Receiver & receiver)
    {
      Communication::LinkedBuffer * buffer = receiver.getBuffer(false);
      while(buffer != 0)
      {
        receiver.releaseBuffer(buffer);
        ++services_;
        buffer = receiver.getBuffer(false);
      }
      return true;
    }
  };
}

/// @brief Stopping from another thread must be seen, and must not race.
TEST(QuickFAST, testStopFromAnotherThreadIsObserved)
{
  EndlessReceiver receiver;
  SilentLogger logger;
  CountingAssembler assembler(
    Codecs::TemplateRegistryPtr(new Codecs::TemplateRegistry), logger);
  ASSERT_TRUE(receiver.start(assembler, 256, 4));
  receiver.runThreads(1, false);

  // Let the service thread get well inside its loops before interfering.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while(assembler.services_.load() < 10
    && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::yield();
  }

  receiver.pause();
  receiver.resume();
  receiver.stop();
  receiver.joinThreads();

  EXPECT_TRUE(receiver.stopping());
  EXPECT_GT(assembler.services_.load(), 0u);
}
