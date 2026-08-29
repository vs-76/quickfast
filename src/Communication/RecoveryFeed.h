// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifndef RECOVERYFEED_H
#define RECOVERYFEED_H
#include "RecoveryFeed_fwd.h"
#include <Communication/LinkedBuffer.h>
#include <chrono>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief An abstract base class for a source of packets used to recover during arbitrage.
    class RecoveryFeed
    {
    public:
      virtual ~RecoveryFeed() { }

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
      void waitGapFill(std::chrono::milliseconds timeout)
      {
        std::unique_lock<std::mutex> lock(inputMutex_);
        if(inputBuffers_.isEmpty())
        {
          inputWait_.wait_until(lock, std::chrono::steady_clock::now() + timeout);
        }
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
      /// if no buffer is available, wait "forever"
      Communication::LinkedBuffer * waitFreeBuffer()
      {
        std::unique_lock<std::mutex> lock(freeMutex_);
        Communication::LinkedBuffer * buffer = freeBuffers_.pop();
        while(buffer == 0)
        {
          freeWait_.wait(lock);
          buffer = freeBuffers_.pop();
        }
        return buffer;
      }

    protected:
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
