// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MULTICASTSOURCEOPTIONS_H
#define MULTICASTSOURCEOPTIONS_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include <Communication/AsioService.h>

#if !defined(_WIN32)
# include <netinet/in.h>
# include <arpa/inet.h>
#endif // _WIN32

/// @brief 1 when the platform can filter multicast by source in the kernel.
///
/// IGMPv3 source filtering is an operating system service, not something Asio
/// offers: the library stops at IP_ADD_MEMBERSHIP.  Every platform QuickFAST
/// targets has the source-specific calls, so the fallback exists to keep the
/// code compiling somewhere unusual rather than because it is expected to run.
#if defined(IP_ADD_SOURCE_MEMBERSHIP) && defined(IP_DROP_SOURCE_MEMBERSHIP)
# define QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST 1
#else
# define QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST 0
#endif

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Asio socket options for source-specific multicast (IPv4).
    namespace MulticastSource
    {
#if QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST

      /// @brief A settable socket option carrying an ip_mreq_source.
      ///
      /// Models Asio's settable socket option: level(), name(), data(), and
      /// size() are all Asio asks of an option passed to set_option().
      ///
      /// The three addresses are written by member name rather than
      /// positionally, because the layout of ip_mreq_source is not portable.
      /// Linux orders it {multiaddr, interface, sourceaddr} while Windows and
      /// the BSDs order it {multiaddr, sourceaddr, interface}, so aggregate
      /// initialization would silently subscribe to the wrong thing on half
      /// the platforms QuickFAST builds on.
      ///
      /// @tparam OptionName is IP_ADD_SOURCE_MEMBERSHIP or IP_DROP_SOURCE_MEMBERSHIP.
      ///
      /// @par Example
      /// @code
      /// MulticastSource::join_source_group join(group, source, nic);
      /// socket.set_option(join);
      /// @endcode
      template<int OptionName>
      class basic_source_membership
      {
      public:
        /// @brief Construct a membership request.
        /// @param group is the multicast group to subscribe to
        /// @param source is the only sender whose traffic is wanted
        /// @param interfaceAddress selects the NIC; 0.0.0.0 lets the system choose
        basic_source_membership(
          const asio::ip::address_v4 & group,
          const asio::ip::address_v4 & source,
          const asio::ip::address_v4 & interfaceAddress)
        {
          std::memset(&value_, 0, sizeof(value_));
          value_.imr_multiaddr.s_addr = ::htonl(group.to_uint());
          value_.imr_sourceaddr.s_addr = ::htonl(source.to_uint());
          value_.imr_interface.s_addr = ::htonl(interfaceAddress.to_uint());
        }

        /// @brief Protocol level of this option: IPPROTO_IP.
        template<typename Protocol>
        int level(const Protocol &) const
        {
          return IPPROTO_IP;
        }

        /// @brief Name of this option: the OptionName template argument.
        template<typename Protocol>
        int name(const Protocol &) const
        {
          return OptionName;
        }

        /// @brief The ip_mreq_source to hand to setsockopt.
        template<typename Protocol>
        const void * data(const Protocol &) const
        {
          return &value_;
        }

        /// @brief Size of the ip_mreq_source.
        template<typename Protocol>
        std::size_t size(const Protocol &) const
        {
          return sizeof(value_);
        }

      private:
        ::ip_mreq_source value_;
      };

      /// @brief Subscribe to one source within a multicast group.
      typedef basic_source_membership<IP_ADD_SOURCE_MEMBERSHIP> join_source_group;
      /// @brief Drop one source from a multicast group subscription.
      typedef basic_source_membership<IP_DROP_SOURCE_MEMBERSHIP> leave_source_group;

#endif // QUICKFAST_HAS_SOURCE_SPECIFIC_MULTICAST

      /// @brief Is an address in the source-specific multicast range?
      ///
      /// RFC 4607 reserves 232.0.0.0/8 for SSM.  Joining a source outside that
      /// range works on the common stacks, but nothing in the standards says a
      /// router has to honour the filter, so it is worth saying out loud.
      ///
      /// @param group is the multicast group to test
      /// @returns true if the group is within 232.0.0.0/8
      inline bool isSourceSpecificRange(const asio::ip::address_v4 & group)
      {
        return (group.to_uint() & 0xFF000000u) == 0xE8000000u;
      }
    }
  }
}
#endif // MULTICASTSOURCEOPTIONS_H
