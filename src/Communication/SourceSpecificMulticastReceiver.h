// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef SOURCESPECIFICMULTICASTRECEIVER_H
#define SOURCESPECIFICMULTICASTRECEIVER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "SourceSpecificMulticastReceiver_fwd.h"
#include <Communication/MulticastReceiverBase.h>
#include <Communication/MulticastSourceOptions.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief A feed that accepts traffic from named senders only.
    ///
    /// Source-specific multicast (RFC 4607): the subscription names the group
    /// and the senders, so the network delivers a channel (S,G) rather than a
    /// group.  Two things follow that an any-source feed does not have to
    /// care about.
    ///
    /// A group may carry more than one permitted sender -- redundant A/B
    /// publishers are the usual reason -- so the feed joins a list of sources
    /// on one socket rather than needing a socket per sender.  A join that
    /// fails partway leaves nothing behind: the sources already joined are
    /// dropped before the failure is reported, because a socket holding half
    /// a subscription would go on delivering traffic the caller was told it
    /// would not get.
    ///
    /// Arriving packets are checked against the source list even though the
    /// kernel was asked to do the filtering.  Source filtering needs IGMPv3
    /// end to end, and a switch or router that falls back to IGMPv2 downgrades
    /// the subscription to any-source without telling anyone; the check costs
    /// one or two address comparisons per packet and turns a silent
    /// misdelivery into a counter.
    class SourceSpecificMulticastFeed
      : public MulticastFeedBase
    {
    public:
      /// @brief The permitted senders for a feed.
      typedef std::vector<std::string> SourceList;

      /// @brief Construct a feed.
      /// @param host is the receiver that owns this feed
      /// @param ioService services the asynchronous reads
      /// @param name identifies this feed in display/log messages
      /// @param multicastGroupIP multicast address as a text string
      /// @param sourceIPs the senders to subscribe to; at least one is required
      /// @param listenInterfaceIP identifies the network interface to be used.
      ///        0.0.0.0 means "let the system choose"
      /// @param bindIP the IP to be used to bind the socket
      /// @param portNumber port number
      /// @throws std::invalid_argument if the source list is empty, or if any
      ///         of the addresses is not IPv4
      SourceSpecificMulticastFeed(
        MulticastFeedHost & host,
        AsioService & ioService,
        const std::string & name,
        const std::string & multicastGroupIP,
        const SourceList & sourceIPs,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
        : MulticastFeedBase(
            host,
            ioService,
            name,
            multicastGroupIP,
            listenInterfaceIP,
            bindIP,
            portNumber)
      {
        if(sourceIPs.empty())
        {
          throw std::invalid_argument(
            "Source specific multicast feed " + name + " needs at least one source address");
        }
        requireV4(multicastGroup(), "multicast group", name);
        requireV4(listenInterface(), "listen interface", name);
        requireV4(bindAddress(), "bind address", name);
        for(size_t nSource = 0; nSource < sourceIPs.size(); ++nSource)
        {
          asio::ip::address source = asio::ip::make_address(sourceIPs[nSource]);
          requireV4(source, "source address", name);
          sources_.push_back(source.to_v4());
        }
      }

      /// @brief The senders this feed subscribes to.
      const std::vector<asio::ip::address_v4> & sources()const
      {
        return sources_;
      }

      /// @brief Is the source list enforced by the network stack?
      ///
      /// False means the platform has no source-specific membership calls, so
      /// the feed holds an ordinary any-source subscription and drops
      /// unwanted senders itself.
      ///
      /// @returns true if the kernel was asked to filter by source.
      static bool kernelSourceFilter()
      {
        return QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST != 0;
      }

    protected:
      // Implement MulticastFeedBase
      virtual void join()
      {
#if QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST
        size_t joined = 0;
        try
        {
          for(joined = 0; joined < sources_.size(); ++joined)
          {
            MulticastSource::join_source_group joinRequest(
              multicastGroup().to_v4(),
              sources_[joined],
              listenInterface().to_v4());
            socket().set_option(joinRequest);
          }
        }
        catch(...)
        {
          dropSources(joined);
          throw;
        }
#else
        asio::ip::multicast::join_group joinRequest(
          multicastGroup().to_v4(),
          listenInterface().to_v4());
        socket().set_option(joinRequest);
#endif // QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST
      }

      virtual void leave()
      {
#if QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST
        dropSources(sources_.size());
#else
        asio::ip::multicast::leave_group leaveRequest(
          multicastGroup().to_v4(),
          listenInterface().to_v4());
        socket().set_option(leaveRequest);
#endif // QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST
      }

      virtual bool acceptSender(const asio::ip::udp::endpoint & sender) const
      {
        const asio::ip::address & address = sender.address();
        if(!address.is_v4())
        {
          return false;
        }
        const asio::ip::address_v4 v4 = address.to_v4();
        for(size_t nSource = 0; nSource < sources_.size(); ++nSource)
        {
          if(sources_[nSource] == v4)
          {
            return true;
          }
        }
        return false;
      }

    private:
      static void requireV4(
        const asio::ip::address & address,
        const char * role,
        const std::string & name)
      {
        if(!address.is_v4())
        {
          throw std::invalid_argument(
            "Source specific multicast feed " + name + ": " + role + " "
            + address.to_string() + " is not IPv4");
        }
      }

#if QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST
      /// Drop the first "count" sources, ignoring errors: this runs while
      /// leaving, or while unwinding a join that already failed, and in both
      /// cases there is nothing useful left to report.
      void dropSources(size_t count)
      {
        for(size_t nSource = 0; nSource < count; ++nSource)
        {
          MulticastSource::leave_source_group leaveRequest(
            multicastGroup().to_v4(),
            sources_[nSource],
            listenInterface().to_v4());
          asio::error_code ec;
          socket().set_option(leaveRequest, ec);
        }
      }
#endif // QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST

    private:
      std::vector<asio::ip::address_v4> sources_;
    };

    /// @brief Receive source-specific multicast packets and pass them to a packet handler.
    ///
    /// Differs from MulticastReceiver only in what it subscribes to: each feed
    /// names the senders it will accept, and traffic from anyone else is
    /// discarded rather than decoded.  Everything else -- buffers, threading,
    /// statistics, the assembler interface -- is the same.
    ///
    /// @par Example
    /// @code
    /// Communication::SourceSpecificMulticastReceiver receiver(
    ///   "232.1.1.1", "10.0.0.42", "10.0.0.1", "", 13000);
    /// receiver.start(assembler);
    /// receiver.runThreads();
    /// @endcode
    class SourceSpecificMulticastReceiver
      : public MulticastReceiverBase
    {
    public:
      /// @brief The permitted senders for a feed.
      typedef SourceSpecificMulticastFeed::SourceList SourceList;

      /// @brief Construct
      SourceSpecificMulticastReceiver()
        : MulticastReceiverBase()
      {
      }

      /// @brief Construct given shared io_service
      /// @param ioService to be used for this receiver
      SourceSpecificMulticastReceiver(asio::io_context & ioService)
        : MulticastReceiverBase(ioService)
      {
      }

      /// @brief Convenience constructor: construct and configure a single-source feed
      /// @param multicastGroupIP multicast address as a text string
      /// @param sourceIP the only sender whose traffic will be accepted
      /// @param listenInterfaceIP listen address as a text string.
      ///        This identifies the network interface to be used.
      ///        0.0.0.0 means "let the system choose"
      /// @param bindIP the IP to be used to bind the socket.  Empty means
      ///        bind to the group address.
      /// @param portNumber port number
      SourceSpecificMulticastReceiver(
        const std::string & multicastGroupIP,
        const std::string & sourceIP,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
        : MulticastReceiverBase()
      {
        addFeed(
          "default",
          multicastGroupIP,
          SourceList(1, sourceIP),
          listenInterfaceIP,
          bindIP,
          portNumber
        );
      }

      /// @brief Convenience constructor: construct and configure a single-source feed w/ specific I/O service
      /// @param ioService to be used for this receiver
      /// @param multicastGroupIP multicast address as a text string
      /// @param sourceIP the only sender whose traffic will be accepted
      /// @param listenInterfaceIP listen address as a text string
      /// @param bindIP the IP to be used to bind the socket.  Empty means
      ///        bind to the group address.
      /// @param portNumber port number
      SourceSpecificMulticastReceiver(
        asio::io_context & ioService,
        const std::string & multicastGroupIP,
        const std::string & sourceIP,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
        : MulticastReceiverBase(ioService)
      {
        addFeed(
          "default",
          multicastGroupIP,
          SourceList(1, sourceIP),
          listenInterfaceIP,
          bindIP,
          portNumber
        );
      }

      ~SourceSpecificMulticastReceiver()
      {
      }

      /// @brief Add a new feed
      ///
      /// Warning: All feeds must be added before initializeReceiver is called.
      ///
      /// @param name to identify this feed in display/log messages
      /// @param multicastGroupIP multicast address as a text string
      /// @param sourceIPs the senders to subscribe to; at least one is required
      /// @param listenInterfaceIP listen address as a text string.
      ///        This identifies the network interface to be used.
      ///        0.0.0.0 means "let the system choose"
      /// @param bindIP the IP to be used to bind the socket.  Empty means bind
      ///        to the group address, which is what a source-specific
      ///        subscription usually wants: binding to the interface address
      ///        instead makes some stacks drop the group traffic.
      /// @param portNumber port number
      void addFeed(
        const std::string & name,
        const std::string & multicastGroupIP,
        const SourceList & sourceIPs,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
      {
        MulticastFeedPtr feed(new SourceSpecificMulticastFeed(
          feedHost(),
          ioService_,
          name,
          multicastGroupIP,
          sourceIPs,
          listenInterfaceIP,
          bindIP.empty() ? multicastGroupIP : bindIP,
          portNumber));
        MulticastReceiverBase::addFeed(feed);
      }

      // Implement Receiver method
      virtual bool initializeReceiver()
      {
        warnAboutGroupsOutsideTheSSMRange();
        return MulticastReceiverBase::initializeReceiver();
      }

    private:
      void warnAboutGroupsOutsideTheSSMRange()
      {
        if(!assembler_->wantLog(Common::Logger::QF_LOG_WARNING))
        {
          return;
        }
        for(size_t nFeed = 0; nFeed < feedCount(); ++nFeed)
        {
          MulticastFeedBase & candidate = feed(nFeed);
          const asio::ip::address group = candidate.multicastGroup();
          if(group.is_v4() && !MulticastSource::isSourceSpecificRange(group.to_v4()))
          {
            std::stringstream msg;
            msg << "Feed " << candidate.name() << ": group " << group.to_string()
              << " is outside the source specific range 232.0.0.0/8."
              << "  Source filtering depends on the network honoring IGMPv3 there.";
            assembler_->logMessage(Common::Logger::QF_LOG_WARNING, msg.str());
          }
        }
      }
    };
  }
}
#endif // SOURCESPECIFICMULTICASTRECEIVER_H
