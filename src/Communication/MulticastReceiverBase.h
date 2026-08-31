// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MULTICASTRECEIVERBASE_H
#define MULTICASTRECEIVERBASE_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include <Communication/AsynchReceiver.h>
#include <Communication/MulticastFeedBase.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Receive multicast packets from one or more feeds.
    ///
    /// Owns the feeds and drives them: start, stop, pause, resume, and the
    /// round-robin that picks which feed reads into the next free buffer.
    /// What kind of subscription each feed holds is the feed's business, so
    /// a derived receiver adds nothing but a way to create its own flavour
    /// of feed.
    ///
    /// @see MulticastReceiver for any-source multicast.
    /// @see SourceSpecificMulticastReceiver for source-specific multicast.
    class MulticastReceiverBase
      : public AsynchReceiver
      , private MulticastFeedHost
    {
    public:
      /// @brief Construct using the common io service.
      MulticastReceiverBase()
        : AsynchReceiver()
      {
      }

      /// @brief Construct given a shared io service.
      /// @param ioService to be used for this receiver
      MulticastReceiverBase(asio::io_context & ioService)
        : AsynchReceiver(ioService)
      {
      }

      ~MulticastReceiverBase()
      {
      }

      /// @brief How many feeds have been added?
      size_t feedCount() const
      {
        return feeds_.size();
      }

      /// @brief Statistic: packets discarded because they came from an unexpected sender.
      ///
      /// Always zero unless a feed filters by sender.  These packets are also
      /// counted as empty packets, because that is how they are recycled.
      ///
      /// @returns the number of packets discarded across all feeds.
      size_t rejectedPackets() const
      {
        size_t total = 0;
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          total += feeds_[nFeed]->rejectedPackets();
        }
        return total;
      }

      // Implement Receiver method
      virtual bool initializeReceiver()
      {
        bool ok = true;
        size_t nFeed = 0;
        try
        {
          for(nFeed = 0; ok && nFeed < feeds_.size(); ++nFeed)
          {
            if(assembler_->wantLog(Common::Logger::QF_LOG_INFO))
            {
              std::stringstream msg;
              msg << "Joining multicast group for feed " << feeds_[nFeed]->name()
                << ": " << feeds_[nFeed]->multicastGroup().to_string()
                << " via interface " << feeds_[nFeed]->endpoint().address().to_string()
                << ':' << feeds_[nFeed]->endpoint().port();
              assembler_->logMessage(Common::Logger::QF_LOG_INFO, msg.str());
            }
            feeds_[nFeed]->initializeReceiver();
          }
        }
        catch (const std::exception & exception)
        {
          ok = false;
          std::stringstream msg;
          msg << "Error " << exception.what()
            << " joining multicast group ";
          if(nFeed < feeds_.size())
          {
            msg << " for feed " << feeds_[nFeed]->name()
                << ": " << feeds_[nFeed]->multicastGroup().to_string()
                << " via interface " << feeds_[nFeed]->endpoint().address().to_string()
                << ':' << feeds_[nFeed]->endpoint().port();
          }
          assembler_->logMessage(Common::Logger::QF_LOG_SERIOUS, msg.str());
        }
        if(!ok)
        {
          for(size_t nStop = 0; nStop < feeds_.size(); ++nStop)
          {
            feeds_[nStop]->stop();
          }
        }
        return ok;
      }

      virtual void stop()
      {
        // stop processing buffers first
        AsynchReceiver::pause();
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          feeds_[nFeed]->stop();
        }
        // and then shut everything down for good.
        AsynchReceiver::stop();
      }

      virtual void pause()
      {
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          feeds_[nFeed]->pause();
        }
        AsynchReceiver::pause();
      }

      virtual void resume()
      {
        AsynchReceiver::resume();
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          feeds_[nFeed]->resume();
        }
      }

    protected:
      /// @brief Take ownership of a feed created by a derived receiver.
      ///
      /// Warning: all feeds must be added before initializeReceiver is called.
      ///
      /// @param feed to be serviced by this receiver
      void addFeed(MulticastFeedPtr feed)
      {
        feeds_.push_back(feed);
      }

      /// @brief The host interface a feed constructor expects.
      MulticastFeedHost & feedHost()
      {
        return *this;
      }

      /// @brief Access a feed by index.
      /// @param index identifies the feed, in the order the feeds were added
      MulticastFeedBase & feed(size_t index)
      {
        return *feeds_[index];
      }

    private:
      // Implement MulticastFeedHost
      virtual void feedReceived(
        const asio::error_code & error,
        LinkedBuffer * buffer,
        size_t bytesReceived)
      {
        handleReceive(error, buffer, bytesReceived);
      }

      virtual bool feedStopping() const
      {
        return stopping();
      }

      bool fillBuffer(LinkedBuffer * buffer, std::unique_lock<std::mutex> & lock)
      {
        // consider fairness/round robin
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          if(feeds_[nFeed]->canStartRead())
          {
            return feeds_[nFeed]->fillBuffer(buffer, lock);
          }
        }
        assert(false); // we should never get here
        return false;
      }

      virtual bool canStartRead()
      {
        for(size_t nFeed = 0; nFeed < feeds_.size(); ++nFeed)
        {
          if(feeds_[nFeed]->canStartRead())
          {
            return true;
          }
        }
        return false;
      }

    private:
      MulticastFeedVector feeds_;
    };
  }
}
#endif // MULTICASTRECEIVERBASE_H
