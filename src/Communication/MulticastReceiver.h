// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MULTICASTRECEIVER_H
#define MULTICASTRECEIVER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "MulticastReceiver_fwd.h"
#include <Communication/MulticastReceiverBase.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief A feed that accepts traffic from any sender in the group.
    ///
    /// Ordinary (any-source) multicast: the subscription names a group and
    /// an interface, and whoever sends to that group is heard.
    class AnySourceMulticastFeed
      : public MulticastFeedBase
    {
    public:
      /// @brief Construct a feed.
      /// @param host is the receiver that owns this feed
      /// @param ioService services the asynchronous reads
      /// @param name identifies this feed in display/log messages
      /// @param multicastGroupIP multicast address as a text string
      /// @param listenInterfaceIP identifies the network interface to be used
      /// @param bindIP the IP to be used to bind the socket
      /// @param portNumber port number
      AnySourceMulticastFeed(
        MulticastFeedHost & host,
        AsioService & ioService,
        const std::string & name,
        const std::string & multicastGroupIP,
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
      }

    protected:
      // Implement MulticastFeedBase
      virtual void join()
      {
        asio::ip::multicast::join_group joinRequest(
          multicastGroup().to_v4(),
          listenInterface().to_v4());
        socket().set_option(joinRequest);
      }

      virtual void leave()
      {
        asio::ip::multicast::leave_group leaveRequest(
          multicastGroup().to_v4(),
          listenInterface().to_v4());
        socket().set_option(leaveRequest);
      }
    };

    /// @brief Receive Multicast Packets and pass them to a packet handler
    class MulticastReceiver
      : public MulticastReceiverBase
    {
    public:

      /// @brief Construct
      MulticastReceiver()
        : MulticastReceiverBase()
      {
      }

      /// @brief construct given shared io_service
      MulticastReceiver(asio::io_context & ioService)
        : MulticastReceiverBase(ioService)
      {
      }

      /// @brief Convenience constructor: Construct and configure a feed
      /// @param multicastGroupIP multicast address as a text string
      /// @param listenInterfaceIP listen address as a text string.
      ///        This identifies the network interface to be used.
      ///        0.0.0.0 means "let the system choose"
      /// @param bindIP the IP to be used to bind the socket.
      /// @param portNumber port number
      MulticastReceiver(
        const std::string & multicastGroupIP,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
        : MulticastReceiverBase()
      {
        addFeed(
         "default",
          multicastGroupIP,
          listenInterfaceIP,
          bindIP,
          portNumber
        );
      }

      /// @brief Convenience constructor: Construct and configure a feed w/ specific I/O service
      /// @param ioService to be used for this receiver
      /// @param multicastGroupIP multicast address as a text string
      /// @param listenInterfaceIP listen address as a text string
      /// @param bindIP the IP to be used to bind the socket.
      /// @param portNumber port number
      MulticastReceiver(
        asio::io_context & ioService,
        const std::string & multicastGroupIP,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
        : MulticastReceiverBase(ioService)
      {
        addFeed(
         "default",
          multicastGroupIP,
          listenInterfaceIP,
          bindIP,
          portNumber
        );
      }

      ~MulticastReceiver()
      {
      }

      /// @brief Add a new feed
      ///
      /// Warning: All feeds must be added before initializeReceiver is called.
      ///
      /// @param name to identitify this feed in display/log messages.
      /// @param multicastGroupIP multicast address as a text string
      /// @param listenInterfaceIP listen address as a text string.
      ///        This identifies the network interface to be used.
      ///        0.0.0.0 means "let the system choose"
      /// @param bindIP the IP to be used to bind the socket.
      /// @param portNumber port number
      void addFeed(
        const std::string & name,
        const std::string & multicastGroupIP,
        const std::string & listenInterfaceIP,
        const std::string & bindIP,
        unsigned short portNumber
        )
      {
        MulticastFeedPtr feed(new AnySourceMulticastFeed(
          feedHost(),
          ioService_,
          name,
          multicastGroupIP,
          listenInterfaceIP,
          bindIP,
          portNumber));
        MulticastReceiverBase::addFeed(feed);
      }
    };
  }
}
#endif // MULTICASTRECEIVER_H
