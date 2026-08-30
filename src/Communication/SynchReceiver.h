// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef SYNCHRECEIVER_H
#define SYNCHRECEIVER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "SynchReceiver_fwd.h"
#include <Communication/Receiver.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief SynchReceiver base class for receiving incoming data
    class SynchReceiver : public Receiver
    {
    public:
      SynchReceiver()
      {
      }

      ~SynchReceiver()
      {
        Receiver::stop();
        if(bool(thread_))
        {
          thread_->join();
          thread_.reset();
        }
      }

      /// @brief End of file stops reading, not servicing.
      ///
      /// The queued packets are still there and still wanted. tryServiceQueue
      /// calls stop() once it has drained them.
      virtual void endOfInput()
      {
      }

      /// @brief Accept a buffer from a synchronous receiver
      ///
      /// The SynchReceiver implementation should call this method
      /// if the buffer is filled during the fillBuffer() call.
      /// I.e. if synchronous I/O is used to fill the buffer.
      /// For asynchronous I/O use handleReceive() instead.
      ///
      /// The lock from the fill buffer call should be returned
      /// for this call.
      /// If this returns true then the caller should eventually
      /// call tryServiceQueue() -- but NOT from the fill buffer call.
      bool acceptFullBuffer(
        LinkedBuffer * buffer,
        size_t bytesReceived,
        std::unique_lock<std::mutex> & lock
        )
      {
        bool needService = false;
        --readsInProgress_;
        ++packetsReceived_;
        if(bytesReceived > 0)
        {
          ++packetsQueued_;
          largestPacket_ = std::max<size_t>(largestPacket_, bytesReceived);
          buffer->setUsed(bytesReceived);
          needService = queue_.push(buffer, lock);
        }
        else
        {
          // empty buffer? just use it again
          ++emptyPackets_;
          idleBufferPool_.push(buffer);
        }
        return needService;
      }

      /// @brief service the queue from a synchronous receiver.
      size_t tryServiceQueue()
      {
        size_t count = 0;
        bool service = false;
        { // Scope for lock
          std::unique_lock<std::mutex> lock(bufferMutex_);
          service = queue_.startService(lock);
        }
        while(service && !stopping_)
        {
          service = serviceQueue();
          ++count;
        }
        if(inputComplete())
        {
          // The queue is drained, so end of input can finally become a stop.
          // Doing it here rather than where the read failed is the whole of
          // the fix: a file with fewer packets than buffers is read to EOF
          // inside start(), before the caller's event loop has run once.
          stop();
        }
        return count;
      }

      ////////////////////////////////////
      // Implement Receiver public methods
      virtual void run()
      {
        while(!stopping_)
        {
          tryServiceQueue();
        }
      }

      virtual void run_one()
      {
        if(!stopping_)
        {
          tryServiceQueue();
        }
      }

      virtual size_t poll()
      {
        size_t count = 0;
        bool more = true;
        while(more && !stopping_)
        {
          size_t pass =  tryServiceQueue();
          count += pass;
          more = pass != 0;
        }
        return count;
      }

      virtual size_t poll_one()
      {
        size_t count = 0;
        if(!stopping_)
        {
          count += tryServiceQueue();
        }
        return count;
      }

      virtual void runThreads([[maybe_unused]] size_t threadCount = 0, bool useThisThread = true)
      {
        if(useThisThread)
        {
          // If we're using this thread, that's all that is needed.
          // so ignore threadCount
          run();
        }
        else
        {
          // we have to start a thread, but it doesn't make sense to have
          // more than one thread servicing a synchronous data source,
          // so only start the one.
          thread_.reset(
            new std::thread([this]{ this->run(); }));
        }
      }

      virtual void joinThreads()
      {
        if(bool(thread_))
        {
          thread_->join();
          thread_.reset();
        }
      }
      virtual bool waitBuffer()
      {
        // if it's going to be there, it's there.
        return true;
      }

    private:
      std::unique_ptr<std::thread> thread_;
    };
  }
}
#endif // SYNCHRECEIVER_H
