// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef TCPRECEIVER_FWD_H
#define TCPRECEIVER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST{
  namespace Communication{
    class TCPReceiver;
    /// @brief smart pointer to a TCPReceiver
    typedef std::shared_ptr<TCPReceiver> TCPReceiverPtr;
  }
}
#endif // TCPRECEIVER_FWD_H
