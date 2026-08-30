// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldInt64.h"
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

FieldCPtr FieldInt64::nullField_ = FieldCPtr(new FieldInt64);

FieldInt64::FieldInt64(int64 value)
  : Field(ValueType::INT64, true)
{
  signedInteger_ = value;
}

FieldInt64::FieldInt64()
  : Field(ValueType::INT64, false)
{
}

FieldInt64::~FieldInt64()
{
}

int64
FieldInt64::toInt64() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return static_cast<int64>(signedInteger_);
}

FieldCPtr
FieldInt64::create(int64 value)
{
  return FieldCPtr(new FieldInt64(value));
}

FieldCPtr
FieldInt64::createNull()
{
  return nullField_;
}

void
FieldInt64::valueToStringBuffer() const
{
  std::stringstream buffer;
  buffer << signedInteger_;
  string_.assign(reinterpret_cast<const unsigned char *>(buffer.str().data()), buffer.str().size());
}

bool
FieldInt64::isSignedInteger()const
{
  return true;
}
