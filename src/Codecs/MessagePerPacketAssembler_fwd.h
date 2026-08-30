// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MESSAGEPERPACKETASSEMBLER_FWD_H
#define MESSAGEPERPACKETASSEMBLER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST
{
  namespace Codecs
  {
    class MessagePerPacketAssembler;

    ///@brief smart pointer to MessagePerPacketAssembler
    typedef std::shared_ptr<MessagePerPacketAssembler> MessagePerPacketAssemblerPtr;
  }
}
#endif // MESSAGEPERPACKETASSEMBLER_FWD_H
