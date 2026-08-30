// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef HOSTRESOLVER_H
#define HOSTRESOLVER_H
#include <Common/QuickFAST_Export.h>
#include <asio.hpp>
#include <string>
#include <vector>

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Resolve @a host and @a service to TCP endpoints (AF_UNSPEC).
    ///
    /// When QuickFAST is built with @c QUICKFAST_HAVE_CARES, uses c-ares
    /// (hosts file + DNS; no glibc NSS). Otherwise uses Asio's
    /// @c ip::tcp::resolver (@c getaddrinfo).
    ///
    /// Numeric host strings skip name lookup. Service may be a port number
    /// or a TCP service name (resolved by the active backend).
    ///
    /// @param ioContext Asio context (used by the Asio backend).
    /// @param host Hostname or numeric address string.
    /// @param service Port number or TCP service name.
    /// @param ec Set on failure; cleared on success.
    /// @return Endpoints suitable for @c tcp::socket::connect; empty on error.
    QuickFAST_Export std::vector<asio::ip::tcp::endpoint> resolveTcp(
      asio::io_context & ioContext,
      const std::string & host,
      const std::string & service,
      asio::error_code & ec);
  }
}
#endif // HOSTRESOLVER_H
