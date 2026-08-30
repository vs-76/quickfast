// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MESSAGE_FWD_H
#define MESSAGE_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST{
  namespace Messages{
    class Message;
    /// @brief Smart pointer to a Message.
    typedef std::shared_ptr<Message> MessagePtr;
    /// @brief Smart pointer to a const Message.
    typedef std::shared_ptr<const Message> MessageCPtr;
  }
}
#endif // MESSAGE_FWD_H
