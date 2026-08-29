// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef SOURCESPECIFICMULTICASTRECEIVER_FWD_H
#define SOURCESPECIFICMULTICASTRECEIVER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST{
  namespace Communication{
    class SourceSpecificMulticastReceiver;
    /// @brief smart pointer to a SourceSpecificMulticastReceiver
    typedef std::shared_ptr<SourceSpecificMulticastReceiver> SourceSpecificMulticastReceiverPtr;
  }
}
#endif // SOURCESPECIFICMULTICASTRECEIVER_FWD_H
