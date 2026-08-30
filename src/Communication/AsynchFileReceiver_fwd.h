// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef ASYNCHFILERECEIVER_FWD_H
#define ASYNCHFILERECEIVER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST{
  namespace Communication{
#if defined(_WIN32)
    class AsynchFileReceiver;
    /// @brief smart pointer to a AsynchFileReceiver
    typedef std::shared_ptr<AsynchFileReceiver> AsynchFileReceiverPtr;
#endif // _WIN32
  }
}
#endif // ASYNCHFILERECEIVER_FWD_H
