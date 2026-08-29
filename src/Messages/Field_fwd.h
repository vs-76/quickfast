// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef FIELD_FWD_H
#define FIELD_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

#include <Common/QuickFAST_Export.h>
#include <memory>

namespace QuickFAST{
  namespace Messages{
    class Field;
    /// @brief A shared pointer to a const Field
    typedef std::shared_ptr<const Field> FieldCPtr;
  }
}
#endif // FIELD_FWD_H
