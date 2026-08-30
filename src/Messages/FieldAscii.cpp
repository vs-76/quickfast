// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldAscii.h"
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

FieldCPtr FieldAscii::nullField_ = FieldCPtr(new FieldAscii);
FieldCPtr FieldAscii::emptyField_ = FieldCPtr(new FieldAscii(std::string()));

FieldAscii::FieldAscii(const std::string & value)
  : Field(ValueType::ASCII, true)
{
  string_ = value;
}

FieldAscii::FieldAscii(const uchar * value, size_t length)
  : Field(ValueType::ASCII, true)
{
  string_.assign(value, length);
}

FieldAscii::FieldAscii()
  : Field(ValueType::ASCII, false)
{
}

FieldAscii::~FieldAscii()
{
}

bool
FieldAscii::isString() const
{
  return true;
}

const StringBuffer &
FieldAscii::toAscii() const
{
  return toString();
}

FieldCPtr
FieldAscii::create(const std::string & value)
{
  if(value.empty())
  {
    return emptyField_;
  }
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
  Field::noteHeapCreate();
#endif
  return FieldCPtr(new FieldAscii(value));
}

FieldCPtr
FieldAscii::create(const uchar * buffer, size_t length)
{
  if(length == 0)
  {
    return emptyField_;
  }
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
  Field::noteHeapCreate();
#endif
  return FieldCPtr(new FieldAscii(buffer, length));
}

FieldCPtr
FieldAscii::createNull()
{
  return nullField_;
}
