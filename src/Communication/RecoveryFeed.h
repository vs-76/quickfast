// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifndef RECOVERYFEED_H
#define RECOVERYFEED_H
#include "RecoveryFeed_fwd.h"
#include <Communication/LinkedBuffer.h>
#include <atomic>
#include <chrono>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief An abstract base class for a source of packets used to recover during arbitrage.
    ///
    /// A derived class is responsible for seeding the free pool: call
    /// releaseBuffer() for each buffer the feed owns before the producer
    /// thread starts. A feed that never seeds the pool will park in
    /// waitFreeBuffer() until stop() is called, and getFreeBuffer() will
    /// return 0 forever.
    class RecoveryFeed
    {
    public:
      virtual ~RecoveryFeed() { }

      /// @brief Release every thread waiting on this feed and keep it released.
      ///
      /// Safe to call from any thread, and more than once.
      ///
      /// @par Example
      /// @code
      /// feed.stop();       // the producer thread's waitFreeBuffer returns 0
      /// producer.join();
      /// @endcode
      void stop()
      {
        stopping_ = true;
        freeWait_.notify_all();
        inputWait_.notify_all();
      }

      /// @brief Has stop() been called?
      /// @returns true once the feed is shutting down.
      bool stopping() const
      {
        return stopping_;
      }

      /// @brief Report that a gap in input sequence numbers has been detected.
      /// @returns true if the gap will be filled; false if the gap should be skipped
      virtual bool reportGap(sequence_t firstMissing, sequence_t firstPresentAfterGap) = 0;

      /// @brief A reminder from the Arbitrage process that a gap still exists.
      ///
      /// This gap has been reported previously via reportGap()
      /// The gap reported by this call may be smaller than what was previously reported
      /// if new packets have arrived, but it will never be larger.
      /// @returns true if the gap will be filled; false if the gap should be skipped
      virtual bool stillWaiting(sequence_t firstMissing, sequence_t firstPresentAfterGap) = 0;

      /// @brief Accept incoming packet
      void acceptBuffer(Communication::LinkedBuffer * buffer)
      {
        {
          std::unique_lock<std::mutex> lock(inputMutex_);
          inputBuffers_.push(buffer);
        }
        inputWait_.notify_all();
      }

      /// @brief transfer all incoming packets into the supplied queue.
      ///
      /// @param queue to hold incoming packets
      void fetchBuffers(Communication::BufferQueue & queue)
      {
        std::unique_lock<std::mutex> lock(inputMutex_);
        queue.push(inputBuffers_);
      }

      /// @brief Wait for a gap fill or a timeout.
      /// No guarantee that the gap will be filled.
      /// @param timeout is how long to wait.
      /// @returns true if recovery data arrived; false on timeout or shutdown.
      ///
      /// @par Example
      /// @code
      /// if(!feed.waitGapFill(std::chrono::milliseconds(10)))
      /// {
      ///   ++consecutiveTimeouts; // escalate rather than poll forever
      /// }
      /// @endcode
      bool waitGapFill(std::chrono::milliseconds timeout)
      {
        std::unique_lock<std::mutex> lock(inputMutex_);
        // The predicated wait_until evaluates its predicate before blocking,
        // so it already covers the data-already-present case.
        return inputWait_.wait_until(
          lock,
          std::chrono::steady_clock::now() + timeout,
          [this]{ return !inputBuffers_.isEmpty() || stopping_; })
          && !inputBuffers_.isEmpty();
      }

      /// @brief accept a buffer after its contents have been used.
      ///
      /// @param buffer is the now-empty buffer ready for re-use.
      void releaseBuffer(Communication::LinkedBuffer * buffer)
      {
        {
          std::unique_lock<std::mutex> lock(freeMutex_);
          freeBuffers_.push(buffer);
        }
        freeWait_.notify_one();
      }

      /// @brief get an empty buffer into which data can be put.
      /// if no buffer is available, return 0
      Communication::LinkedBuffer * getFreeBuffer()
      {
        std::unique_lock<std::mutex> lock(freeMutex_);
        return freeBuffers_.pop();
      }

      /// @brief wait for an empty buffer into which data can be put.
      /// @returns a buffer, or 0 if the feed was stopped while waiting.
      Communication::LinkedBuffer * waitFreeBuffer()
      {
        std::unique_lock<std::mutex> lock(freeMutex_);
        Communication::LinkedBuffer * buffer = freeBuffers_.pop();
        while(buffer == 0 && !stopping_)
        {
          freeWait_.wait(lock, [this]{ return !freeBuffers_.isEmpty() || stopping_; });
          buffer = freeBuffers_.pop();
        }
        return buffer;
      }

    protected:
      /// Set by stop() to release every wait on this feed
      std::atomic<bool> stopping_ = false;
      /// Protects inputBuffers_
      std::mutex inputMutex_;
      /// Waits for inputBuffers_
      std::condition_variable inputWait_;
      /// Buffers ready to be delivered to Assembler
      Communication::BufferQueue inputBuffers_;
      /// Protects freeBuffers_
      std::mutex freeMutex_;
      /// Waits for freeBuffers_
      std::condition_variable freeWait_;
      /// Empty buffers returned from Assembler
      Communication::BufferQueue freeBuffers_;
    };
  }
}

#endif // CUSTOMFEED_FWD_H
