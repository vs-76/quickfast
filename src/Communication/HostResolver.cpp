// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "HostResolver.h"

#if defined(QUICKFAST_HAVE_CARES) && QUICKFAST_HAVE_CARES
# include <ares.h>
# include <cstring>
# if defined(_WIN32)
#  include <winsock2.h>
# endif
#endif

namespace QuickFAST
{
  namespace Communication
  {
    namespace
    {
      /// @brief True when @a host is a literal IPv4 or IPv6 address.
      bool isNumericHost(const std::string & host)
      {
        asio::error_code ignore;
        (void) asio::ip::make_address(host, ignore);
        return !ignore;
      }

      /// @brief Map a decimal service string to a port; leave @a ec unset on success.
      bool parseNumericPort(
        const std::string & service,
        unsigned short & port,
        asio::error_code & ec)
      {
        try
        {
          const unsigned long value = std::stoul(service);
          if(value > 65535UL)
          {
            ec = asio::error::service_not_found;
            return false;
          }
          port = static_cast<unsigned short>(value);
          return true;
        }
        catch(const std::exception &)
        {
          return false;
        }
      }
    }

#if defined(QUICKFAST_HAVE_CARES) && QUICKFAST_HAVE_CARES
    namespace
    {
      asio::error_code mapAresStatus(int status)
      {
        switch(status)
        {
        case ARES_SUCCESS:
          return {};
        case ARES_ENOTFOUND:
        case ARES_ENODATA:
          return asio::error::host_not_found;
        case ARES_ETIMEOUT:
          return asio::error::timed_out;
        case ARES_ECONNREFUSED:
          return asio::error::connection_refused;
        case ARES_ENOMEM:
          return asio::error::no_memory;
        default:
          return asio::error::host_not_found_try_again;
        }
      }

      bool ensureCAresLibrary(asio::error_code & ec)
      {
        static const int initStatus = ares_library_init(ARES_LIB_INIT_ALL);
        struct LibraryCleanup
        {
          ~LibraryCleanup()
          {
            ares_library_cleanup();
          }
        };
        static const LibraryCleanup cleanup;
        (void) cleanup;
        if(initStatus != ARES_SUCCESS)
        {
          ec = mapAresStatus(initStatus);
          return false;
        }
        return true;
      }

      struct GetAddrInfoState
      {
        int status{ARES_SUCCESS};
        ares_addrinfo * result{nullptr};
      };

      void onGetAddrInfo(
        void * arg,
        int status,
        int /*timeouts*/,
        struct ares_addrinfo * result)
      {
        auto * state = static_cast<GetAddrInfoState *>(arg);
        state->status = status;
        state->result = result;
      }

      std::vector<asio::ip::tcp::endpoint> resolveWithCAres(
        const std::string & host,
        const std::string & service,
        asio::error_code & ec)
      {
        if(!ensureCAresLibrary(ec))
        {
          return {};
        }
        if(!ares_threadsafety())
        {
          ec = asio::error::operation_not_supported;
          return {};
        }

        ares_channel channel = nullptr;
        struct ares_options options;
        std::memset(&options, 0, sizeof(options));
        // Default lookups include the hosts file then DNS ("fb").
        options.evsys = ARES_EVSYS_DEFAULT;
        const int optmask = ARES_OPT_EVENT_THREAD;
        const int initStatus = ares_init_options(&channel, &options, optmask);
        if(initStatus != ARES_SUCCESS)
        {
          ec = mapAresStatus(initStatus);
          return {};
        }

        ares_addrinfo_hints hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = 0;

        GetAddrInfoState state;
        ares_getaddrinfo(
          channel,
          host.c_str(),
          service.empty() ? nullptr : service.c_str(),
          &hints,
          &onGetAddrInfo,
          &state);

        const ares_status_t waitStatus = ares_queue_wait_empty(channel, -1);
        if(waitStatus != ARES_SUCCESS && waitStatus != ARES_ETIMEOUT)
        {
          if(state.result != nullptr)
          {
            ares_freeaddrinfo(state.result);
          }
          ares_destroy(channel);
          ec = mapAresStatus(static_cast<int>(waitStatus));
          return {};
        }

        std::vector<asio::ip::tcp::endpoint> endpoints;
        if(state.status != ARES_SUCCESS || state.result == nullptr)
        {
          ec = mapAresStatus(state.status == ARES_SUCCESS
            ? ARES_ENOTFOUND
            : state.status);
          if(state.result != nullptr)
          {
            ares_freeaddrinfo(state.result);
          }
          ares_destroy(channel);
          return {};
        }

        for(const ares_addrinfo_node * node = state.result->nodes;
            node != nullptr;
            node = node->ai_next)
        {
          if(node->ai_addr == nullptr)
          {
            continue;
          }
          if(node->ai_family == AF_INET &&
             node->ai_addrlen >= static_cast<ares_socklen_t>(sizeof(sockaddr_in)))
          {
            const auto * sin = reinterpret_cast<const sockaddr_in *>(node->ai_addr);
            endpoints.emplace_back(
              asio::ip::address_v4(ntohl(sin->sin_addr.s_addr)),
              ntohs(sin->sin_port));
          }
          else if(node->ai_family == AF_INET6 &&
                  node->ai_addrlen >= static_cast<ares_socklen_t>(sizeof(sockaddr_in6)))
          {
            const auto * sin6 = reinterpret_cast<const sockaddr_in6 *>(node->ai_addr);
            asio::ip::address_v6::bytes_type bytes;
            std::memcpy(bytes.data(), sin6->sin6_addr.s6_addr, bytes.size());
            endpoints.emplace_back(
              asio::ip::address_v6(bytes, sin6->sin6_scope_id),
              ntohs(sin6->sin6_port));
          }
        }

        ares_freeaddrinfo(state.result);
        ares_destroy(channel);

        if(endpoints.empty())
        {
          ec = asio::error::host_not_found;
          return {};
        }
        ec.clear();
        return endpoints;
      }
    }
#endif // QUICKFAST_HAVE_CARES

#if !(defined(QUICKFAST_HAVE_CARES) && QUICKFAST_HAVE_CARES)
    namespace
    {
      std::vector<asio::ip::tcp::endpoint> resolveWithAsio(
        asio::io_context & ioContext,
        const std::string & host,
        const std::string & service,
        asio::error_code & ec)
      {
        asio::ip::tcp::resolver resolver(ioContext);
        asio::ip::tcp::resolver::flags flags = asio::ip::tcp::resolver::address_configured;
        if(isNumericHost(host))
        {
          flags |= asio::ip::tcp::resolver::numeric_host;
        }
        const asio::ip::tcp::resolver::results_type results =
          resolver.resolve(host, service, flags, ec);
        if(ec)
        {
          return {};
        }
        return {results.begin(), results.end()};
      }
    }
#endif

    std::vector<asio::ip::tcp::endpoint> resolveTcp(
      asio::io_context & ioContext,
      const std::string & host,
      const std::string & service,
      asio::error_code & ec)
    {
      ec.clear();
      if(host.empty())
      {
        ec = asio::error::invalid_argument;
        return {};
      }

      // Literal IP + numeric port: no name lookup on either backend.
      unsigned short port = 0;
      if(isNumericHost(host) && parseNumericPort(service, port, ec))
      {
        if(ec)
        {
          return {};
        }
        const asio::ip::address address = asio::ip::make_address(host, ec);
        if(ec)
        {
          return {};
        }
        ec.clear();
        return {asio::ip::tcp::endpoint(address, port)};
      }
      if(ec)
      {
        return {};
      }

#if defined(QUICKFAST_HAVE_CARES) && QUICKFAST_HAVE_CARES
      (void) ioContext;
      return resolveWithCAres(host, service, ec);
#else
      return resolveWithAsio(ioContext, host, service, ec);
#endif
    }
  }
}
