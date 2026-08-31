// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldByteVector.h"
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

FieldByteVector::FieldByteVector(const std::string & value)
  : Field(ValueType::BYTEVECTOR, true)
{
  string_ = value;
}


FieldByteVector::FieldByteVector(const uchar * buffer, size_t length)
  : Field(ValueType::BYTEVECTOR, true)
{
  string_.assign(buffer, length);
}

FieldByteVector::FieldByteVector()
  : Field(ValueType::BYTEVECTOR, false)
{
}

FieldByteVector::~FieldByteVector()
{
}

bool
FieldByteVector::isString() const
{
  return true;
}

const StringBuffer &
FieldByteVector::toByteVector() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return string_;
}


FieldCPtr
FieldByteVector::create(const std::string & value)
{
  return FieldCPtr(new FieldByteVector(value));
}

FieldCPtr
FieldByteVector::create(const uchar * buffer, size_t length)
{
  return FieldCPtr(new FieldByteVector(buffer, length));
}

FieldCPtr
FieldByteVector::createNull()
{
  return FieldCPtr(new FieldByteVector);
}

