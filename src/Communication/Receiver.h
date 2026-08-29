// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef RECEIVER_H
#define RECEIVER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "Receiver_fwd.h"
#include <Communication/Assembler.h>
#include <Communication/SingleServerBufferQueue.h>
#include <Common/Exceptions.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Receiver base class for receiving incoming data
    class Receiver
    {
    public:
      /// @brief A counter written by the receiver and read by anyone.
      ///
      /// Most of the counters were incremented under bufferMutex_, but the
      /// public getters read them with no lock and two of them
      /// (packetsProcessed_, bytesProcessed_) were incremented outside the
      /// mutex as well. Aligned size_t reads do not tear on the hardware this
      /// runs on, so the practical damage was limited to stale statistics --
      /// but it is still formally a race, and it is the kind that fills a
      /// sanitizer report with noise that hides the findings worth reading.
      ///
      /// Relaxed ordering throughout: these counters order nothing and
      /// synchronise nothing, so a relaxed load and store compile to what a
      /// plain size_t compiled to. The increments become locked instructions,
      /// which is the whole cost of the change.
      ///
      /// @par Example
      /// @code
      /// Statistic packets;
      /// ++packets;
      /// size_t howMany = packets;
      /// @endcode
      class Statistic
      {
      public:
        /// @brief Construct with an initial value.
        /// @param value is the starting count.
        Statistic(size_t value = 0)
          : value_(value)
        {
        }

        /// @brief Read the counter.
        /// @returns the current count.
        operator size_t() const
        {
          return value_.load(std::memory_order_relaxed);
        }

        /// @brief Set the counter.
        /// @param value is the new count.
        /// @returns this counter.
        Statistic & operator=(size_t value)
        {
          value_.store(value, std::memory_order_relaxed);
          return *this;
        }

        /// @brief Add to the counter.
        /// @param value is the amount to add.
        /// @returns this counter.
        Statistic & operator+=(size_t value)
        {
          value_.fetch_add(value, std::memory_order_relaxed);
          return *this;
        }

        /// @brief Pre-increment.
        /// @returns the new count.
        size_t operator++()
        {
          return value_.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        /// @brief Post-increment.
        /// @returns the count before the increment.
        size_t operator++(int)
        {
          return value_.fetch_add(1, std::memory_order_relaxed);
        }

      private:
        std::atomic<size_t> value_;
      };

      Receiver()
        : assembler_(0)
        , bufferSize_(1500)
        , paused_(false)
        , stopping_(false)
        , readsInProgress_(0)
        , noBufferAvailable_(0)
        , packetsReceived_(0)
        , bytesReceived_(0)
        , errorPackets_(0)
        , pausedPackets_(0)
        , emptyPackets_(0)
        , packetsQueued_(0)
        , batchesProcessed_(0)
        , packetsProcessed_(0)
        , bytesProcessed_(0)
        , largestPacket_(0)
      {
      }

      virtual ~Receiver()
      {
      }

      /// @brief Start accepting packets.  Returns immediately
      /// @param assembler accepts and processes the filled buffers
      /// @param bufferSize determines the maximum size of an incoming packet
      /// @param bufferCount is the number of buffers to allocate to receive packets
      /// @returns true if service started successfully
      bool start(
        Assembler & assembler,
        size_t bufferSize = 1500,
        size_t bufferCount = 2)
      {
        bool result = false;
        assembler_ = & assembler;
        bufferSize_ = bufferSize;
        if(initializeReceiver())
        {
          assembler_->receiverStarted(*this);

          // Allocate initial set of buffers
          std::unique_lock<std::mutex> lock(bufferMutex_);

          for(size_t nBuffer = 0; nBuffer < bufferCount; ++nBuffer)
          {
            BufferLifetime buffer(new LinkedBuffer(bufferSize));
            /// bufferLifetimes_ is used only to clean up on object destruction
            bufferLifetimes_.push_back(buffer);
            idleBufferPool_.push(buffer.get());
          }
          startReceive(lock);
          result = true;
        }
        return result;
      }


      /// @brief The most bytes the buffer pool can hold at one time.
      ///
      /// needBytes cannot ever report more than this many bytes available, so
      /// an assembler asked to wait for a larger block is waiting for
      /// something that cannot happen.
      ///
      /// The pool is measured buffer by buffer rather than as count times
      /// size, because a buffer wrapping external memory reports a capacity of
      /// zero and carries its extent in used() instead.
      ///
      /// @returns the total capacity in bytes, or zero before start().
      size_t totalBufferCapacity() const
      {
        std::unique_lock<std::mutex> lock(bufferMutex_);
        size_t total = 0;
        for(const BufferLifetime & buffer : bufferLifetimes_)
        {
          total += std::max(buffer->capacity(), buffer->used());
        }
        return total;
      }

      /// @brief add additional buffers on-the-fly
      /// @param bufferCount is how many buffers to add
      void addBuffers(
        size_t bufferCount = 1)
      {
        std::unique_lock<std::mutex> lock(bufferMutex_);

        for(size_t nBuffer = 0; nBuffer < bufferCount; ++nBuffer)
        {
          BufferLifetime buffer(new LinkedBuffer(bufferSize_));
          /// bufferLifetimes_ is used only to clean up on object destruction
          bufferLifetimes_.push_back(buffer);
          idleBufferPool_.push(buffer.get());
        }
      }

      ////////////////////////////////////////////////////////////////////
      // public methods to be implemented by specific types of receiver

      /// @brief Stop accepting packets
      ///
      /// Returns immediately, however decoding may continue until
      /// the decoder reaches a clean stopping point.  In particular
      /// the Assembler may receive additional messages after
      /// stop is called.
      ///
      /// Assembler::decodingStopped() will be called when
      /// the stop request is complete.
      virtual void stop()
      {
        stopping_ = true;
      }

      /// @brief Has a stop been requested?
      /// @returns true once stop() has been called.
      bool stopping() const
      {
        return stopping_;
      }

      /// @brief Report that the source has no more data to offer.
      ///
      /// Called when a read fails, which for a synchronous source means end
      /// of file. The default stops the receiver outright, which is right for
      /// an asynchronous source: a failed read there is a broken connection,
      /// not an orderly end, and nothing is waiting in the queue. A
      /// synchronous receiver overrides this so the queue can drain first.
      virtual void endOfInput()
      {
        stop();
      }

      /// @brief Has the source run out of data?
      /// @returns true once a read has failed.
      bool inputComplete() const
      {
        return inputComplete_;
      }

      /// @brief Ignore incoming packets until resume()
      virtual void pause()
      {
        paused_ = true;
      }

      /// @brief Resume handling incoming packets after pause()
      virtual void resume()
      {
        paused_ = false;
      }

      /// @brief Special method for BUFFER_RECEIVER
      ///
      /// Process data from an external buffer;
      virtual void receiveBuffer(
        [[maybe_unused]] const unsigned char * buffer,
        [[maybe_unused]] size_t used
        )
      {
        throw std::logic_error("Unexpected call to Communications::Receiver::receiveBuffer()");
      }

      /// @brief run the event loop in this thread
      ///
      /// Exceptions are caught, logged, and ignored.  The event loop continues.
      virtual void run() = 0;

      /// @brief run the event loop until one event is handled.
      virtual void run_one() = 0;

      /// @brief execute any ready event handlers then return.
      virtual size_t poll() = 0;

      /// @brief execute at most one ready event handler than return.
      virtual size_t poll_one() = 0;

      /// @brief create additional threads to run the event loop
      virtual void runThreads(size_t threadCount = 0, bool useThisThread = true) = 0;

      /// @brief join all additional threads after calling stop()
      ///
      /// If stop() has not been called, this will block "forever".
      virtual void joinThreads() = 0;

      /// @brief Reuse AsioService after calling stop and joinThreads
      ///
      /// The sequence of calls should be:
      /// while(whatever)
      /// {
      ///   runThreads
      ///   stop
      ///   joinThreads
      ///    resetService
      /// }
      /// Note that if the AsioService is being shared this will reset "everybody"
      /// if you need to shut down/restart specific parts of the system, arrange to use
      /// separate AsioService (see the AsyncReceiver constructor that takes an AsioService as an argument.
      virtual void resetService() = 0;

      /////////////////////////////
      // Assembler support routines

      /// @brief Wait for a buffer to arrive
      virtual bool waitBuffer() = 0;

      /// @brief get the next buffer if possible
      /// WARNING: This should be called only by a Assembler
      ///          during a call from this Receiver
      /// @param wait if true, will wait until a buffer is available
      ///        or the service is stopped)
      /// @return pointer to the buffer or zero
      virtual LinkedBuffer * getBuffer(bool wait)
      {
        LinkedBuffer *next = queue_.serviceNext();
        bool more = true;
        while(more && next == 0  && !stopping_)
        {
          more = wait;
          {
            std::unique_lock<std::mutex> lock(bufferMutex_);
            // add any idle buffers to pool
            idleBufferPool_.push(idleBuffers_);
            startReceive(lock);
            queue_.refresh(lock, wait && !stopping_ && !inputComplete_);
          }
          next = queue_.serviceNext();
        }
        if(next != 0)
        {
          ++packetsProcessed_;
          bytesProcessed_ += next->used();
        }
        return next;
      }

      /// @brief check to see if "needed" bytes are available
      /// WARNING: This should be called only by a Assembler
      ///          during a call from this Receiver
      /// @param needed number of bytes needed
      /// @param wait true if we should wait for them to arrive;
      ///             false if we should just return false
      /// @returns true if needed bytes are available.
      bool needBytes(size_t needed, bool wait)
      {
        size_t available = 0;
        bool more = true;
        while(available < needed && more)
        {
          const LinkedBuffer *next = queue_.peekOutgoing();
          while(available < needed && next != 0)
          {
            available += next->used();
            next = next->link();
          }
          if(available < needed && wait)
          {
            std::unique_lock<std::mutex> lock(bufferMutex_);
            // add any idle buffers to pool
            idleBufferPool_.push(idleBuffers_);
            startReceive(lock);
            queue_.refresh(lock, wait && !inputComplete_);
            available = 0;
          }
          else
          {
            more = false;
          }
        }
        return available >= needed;
      }

      /// @brief Release empty buffer.
      /// WARNING: This should be called only by a Assembler
      ///          during a call from this Receiver
      /// @param buffer points to the buffer to be released.
      void releaseBuffer(LinkedBuffer * buffer)
      {
        //std::ostringstream msg;
        //msg << "{" << (void *)this << "} Release buffer " << (void *)buffer << std::endl;
        //std::cout << msg.str();
        idleBuffers_.push(buffer);
      }
      // Assembler support routines
      /////////////////////////////


    protected:
      /// @brief Enter the startReceive method without a lock
      void startReceiveUnlocked()
      {
        std::unique_lock<std::mutex> lock(bufferMutex_);
        startReceive(lock);
      }

      /// @brief Can a read be started now?
      virtual bool canStartRead()
      {
        return readsInProgress_ == 0;
      }

      /// @brief Receive a new buffer full if possible
      /// scoped_lock parameter means a mutex must be locked
      void startReceive(std::unique_lock<std::mutex>& lock)
      {
        bool more = canStartRead();
        while( more && !stopping_)
        {
          more = false;
          LinkedBuffer *buffer = idleBufferPool_.pop();
          if(buffer != 0)
          {
            ++readsInProgress_;
            if(fillBuffer(buffer, lock))
            {
              more = canStartRead();
            }
            else
            {
              idleBufferPool_.push(buffer);
              --readsInProgress_;
              inputComplete_ = true;
              endOfInput();
            }
          }
          else
          {
            //std::ostringstream msg;
            //msg << "{" << (void *)this << "} Trying to read. No buffer available\n";
            //std::cout << msg.str();
            ++noBufferAvailable_;
          }
        }
      }

      ////////////////////////////////////////////////////////////////////
      // protected methods to be implemented by specific types of receiver
    protected:
      /// @brief Do initial communications startup.
      ///
      /// Do not start I/O during this method.
      /// @returns true if started successfully
      virtual bool initializeReceiver() = 0;

      /// @brief Fill a buffer
      ///
      /// Asynchrounous Receivers should arrange for handleReceive()
      /// to be called at the completion of the read started during
      /// this method.  Normally an asynchronous receiver will return
      /// true from this method and report any errors asynchronously
      /// via the error parameter of handleReceive.
      ///
      /// Synchronous Receivers should call acceptFullBuffer() during
      /// this method.  Synchronous Receivers in particular should
      /// return false if data cannot be read.
      ///
      /// @param buffer to be filled
      /// @param lock to be sure we have it.
      /// @returns true if the buffer is, or will be filled.
      virtual bool fillBuffer(
        LinkedBuffer * buffer,
        std::unique_lock<std::mutex>& lock) = 0;


      /////////////
      // Statistics
    public:
      /// @brief Statistic: How many times were all buffers busy?
      /// @returns the number of times no buffers were available to receive packets.
      size_t noBufferAvailable() const
      {
        return noBufferAvailable_;
      }

      /// @brief Statistic: How many packets have been received
      /// @returns the number of packets that have been received
      size_t packetsReceived() const
      {
        return packetsReceived_;
      }

      /// @brief Statistic: How many packets have been queued for processing
      /// @returns the number of packets that have been received
      size_t packetsQueued() const
      {
        return packetsQueued_;
      }

      /// @brief Statistic: How many batches of packets have been processed
      /// @returns the number of batches
      size_t batchesProcessed() const
      {
        return batchesProcessed_;
      }

      /// @brief Statistic: How many packets have been processed
      /// @returns the number of packets that have been processed.
      size_t packetsProcessed() const
      {
        return packetsProcessed_;
      }

      /// @brief Statistic: How many bytes have been processed
      /// @returns the number of bytes that have been processed.
      size_t bytesProcessed() const
      {
        return bytesProcessed_;
      }

      /// @brief Statistic: How many received packets had errors
      /// @returns the number of packets that have encountered communication errors
      size_t packetsWithErrors() const
      {
        return errorPackets_;
      }

      /// @brief Statistic: How many packetes were ignored because this connection was paused
      ///
      /// @returns the number of paused packets.
      size_t pausedPackets() const
      {
        return pausedPackets_;
      }

      /// @brief Statistic: How many received packets were empty
      /// @returns the number of packets that were empty
      size_t emptyPackets() const
      {
        return emptyPackets_;
      }

      /// @brief Statistic: How many bytes have been received
      /// @returns the number of bytes that have been received
      size_t bytesReceived() const
      {
        return bytesReceived_;
      }

      /// @brief Statistic: How big was the largest packet received
      /// @returns the number of bytes in the largest packet
      size_t largestPacket() const
      {
        return largestPacket_;
      }

      /// @brief Approximately how many bytes are waiting to be decoded
      size_t bytesReadable() const
      {
        // todo: we *could* ask the socket how much data is waiting
        return bytesReceived_ - bytesProcessed_;
      }
      // Statistics
      /////////////

    protected:

      /// @brief Service the queeue of full buffers
      ///
      /// Processes one batch of input buffers
      /// See: SingleServerBufferQueue for details about
      /// what a batch is.
      /// @returns true if more service is needed
      bool serviceQueue()
      {
          ++batchesProcessed_;
          if(!assembler_->serviceQueue(*this))
          {
            stop();
          }
          std::unique_lock<std::mutex> lock(bufferMutex_);
          // add idle buffers to pool before trying to start a read.
          //if(!idleBufferPool_.isEmpty())
          //{
          //  std::ostringstream msg;
          //  msg << "{" << (void *)this << "} promote idle buffers." << std::endl;
          //  std::cout << msg.str();
          //}
          //else
          //{
          //  std::ostringstream msg;
          //  msg << "{" << (void *)this << "} no idle buffers available." << std::endl;
          //  std::cout << msg.str();
          //}
          idleBufferPool_.push(idleBuffers_);
          startReceive(lock);
          // see if this thread is still needed to service the queue
          return queue_.endService(!stopping_, lock);
      }

    protected:
      /// The assembler to receive full buffers
      Assembler * assembler_ = nullptr;
      /// Manage the buffers' lifetimes
      BufferLifetimeManager bufferLifetimes_;

      /// Protect access to the SingleServerBufferQueue
      mutable std::mutex bufferMutex_;

      /// @brief Accept buffers from multiple threads and deliver them to a single thread.
      ///
      /// See the documentation of SingleServerBufferQueue for details.
      SingleServerBufferQueue queue_;

      /// @brief accumulate idle buffers while we process the queue
      ///
      /// Don't add them back to idle pool until we're done
      /// this avoids extra locking, and applies some backpressure to the
      /// incoming communication stream (which of course is ignored for multicast)
      BufferCollection idleBuffers_;

      /// @brief Buffers waiting to be filled
      BufferCollection idleBufferPool_;

      /// @brief All buffers have the same size
      size_t bufferSize_;

      /// @brief temporarily ignore incoming packets
      // stop(), pause() and resume() are public, take no lock, and are called
      // from whatever thread drives shutdown; the flags are read on the
      // service thread and, for stopping_, on asio completion threads. As
      // plain bools that is a data race, and the familiar consequence is that
      // nothing tells the compiler the value can change under a spin: the
      // load may be hoisted out of the loop and stop() never observed, so the
      // receiver does not shut down and the multicast socket is never closed.
      std::atomic<bool> paused_;

      /// @brief True when we're trying to shut down
      std::atomic<bool> stopping_;
      /// Set when a read fails; distinct from stopping_ because a drained
      /// queue and an abandoned one are not the same thing.
      std::atomic<bool> inputComplete_{false};

      /// @brief Number of reads in progress (usually zero or one)
      unsigned int readsInProgress_;

      /////////////
      // Statistics
      /// No buffers avaliable when we could have started a read
      Statistic noBufferAvailable_;
      /// Packets accepted (includes error & empty)
      Statistic packetsReceived_;
      /// Bytes in those packets
      Statistic bytesReceived_;
      /// Packets received with errors (usually disconnect or EOF0
      Statistic errorPackets_;
      /// Packets received in error due to a linux bug.
      Statistic pausedPackets_;
      /// Packets containing no data (usually during shutdown)
      Statistic emptyPackets_;
      /// Packets containing valid data: queued to be processed
      Statistic packetsQueued_;
      /// Batches of packets collected by queue_
      Statistic batchesProcessed_;
      /// Individual packets in the batches
      Statistic packetsProcessed_;
      /// Bytes in the processed packets.
      Statistic bytesProcessed_;
      /// Largest single packet received
      Statistic largestPacket_;
    };
  }
}
#endif // RECEIVER_H
