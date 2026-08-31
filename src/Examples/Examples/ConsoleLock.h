// Copyright (c) 2009, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef CONSOLELOCK_H
#define CONSOLELOCK_H
namespace QuickFAST{
  namespace Examples{
    /// @brief a "global" mutex that can be used to protect access to the console.
    class ConsoleLock
    {
    public:
      /// @brief the mutex to be locked before doing console writes.
      static std::mutex consoleMutex;
    };
  }
}
#endif // CONSOLELOCK_H
