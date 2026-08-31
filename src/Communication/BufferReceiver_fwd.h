// Copyright (c) 2009, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef BUFFERRECEIVER_FWD_H
#define BUFFERRECEIVER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST{
  namespace Communication{
    class BufferReceiver;
    /// @brief smart pointer to a BufferReceiver
    typedef std::shared_ptr<BufferReceiver> BufferReceiverPtr;
  }
}
#endif // BUFFERRECEIVER_FWD_H
