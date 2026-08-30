// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldUInt8.h"
#include <Common/Exceptions.h>
using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;


FieldCPtr FieldUInt8::nullField_ = FieldCPtr(new FieldUInt8);

FieldUInt8::FieldUInt8(uchar value)
  : Field(ValueType::UINT8, true)
{
  unsignedInteger_ = value;
}

FieldUInt8::FieldUInt8()
  : Field(ValueType::UINT8, false)
{
}

FieldUInt8::~FieldUInt8()
{
}

uchar
FieldUInt8::toUInt8() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return static_cast<uchar>(unsignedInteger_);
}

FieldCPtr
FieldUInt8::create(uchar value)
{
  return FieldCPtr(new FieldUInt8(value));
}

FieldCPtr
FieldUInt8::createNull()
{
  return nullField_;
}

void
FieldUInt8::valueToStringBuffer() const
{
  std::stringstream buffer;
  buffer << unsignedInteger_;
  string_.assign(reinterpret_cast<const unsigned char *>(buffer.str().data()), buffer.str().size());
}

bool
FieldUInt8::isUnsignedInteger()const
{
  return true;
}
