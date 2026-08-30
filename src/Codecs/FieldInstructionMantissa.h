// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef FIELDINSTRUCTIONMANTISSA_H
#define FIELDINSTRUCTIONMANTISSA_H
#include <Codecs/FieldInstructionInteger.h>

namespace QuickFAST{
  namespace Codecs{
    /// @brief An implementation for the &lt;mantissa> field instruction within a &lt;decimal>.
    typedef FieldInstructionInteger<mantissa_t, ValueType::MANTISSA, true> FieldInstructionMantissa;
    /// @brief a pointer to a FieldInstrucionMantissa
    typedef std::shared_ptr<FieldInstructionMantissa> FieldInstructionMantissaPtr;
    /// @brief a const pointer to a FieldInstrucionMantissa
    typedef std::shared_ptr<const FieldInstructionMantissa> FieldInstructionMantissaCPtr;
  }
}
#endif // FIELDINSTRUCTIONMANTISSA_H
