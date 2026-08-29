// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef ASIOSERVICE_FWD_H
#define ASIOSERVICE_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

// Boost.Asio 1.66+ removed class io_service (now a typedef of io_context).
// Include the real header rather than forward-declaring a class.
#include <boost/asio/io_context.hpp>

namespace QuickFAST
{
  namespace Communication
  {
    class AsioService;
  }
}
#endif // ASIOSERVICE_FWD_H
