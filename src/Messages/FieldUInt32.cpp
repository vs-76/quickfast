// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldUInt32.h"
#include <Common/Exceptions.h>
using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

FieldCPtr FieldUInt32::nullField_ = FieldCPtr(new FieldUInt32);

FieldUInt32::FieldUInt32(uint32 value)
  : Field(ValueType::UINT32, true)
{
  unsignedInteger_ = value;
}

FieldUInt32::FieldUInt32()
  : Field(ValueType::UINT32, false)
{
}

FieldUInt32::~FieldUInt32()
{
}

uint32
FieldUInt32::toUInt32() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return static_cast<uint32>(unsignedInteger_);
}

FieldCPtr
FieldUInt32::create(uint32 value)
{
  return FieldCPtr(new FieldUInt32(value));
}

FieldCPtr
FieldUInt32::createNull()
{
  return nullField_;
}

void
FieldUInt32::valueToStringBuffer() const
{
  std::stringstream buffer;
  buffer << unsignedInteger_;
  string_.assign(reinterpret_cast<const unsigned char *>(buffer.str().data()), buffer.str().size());
}

bool
FieldUInt32::isUnsignedInteger()const
{
  return true;
}
