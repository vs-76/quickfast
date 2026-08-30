// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldInt8.h"
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

namespace
{
  const size_t internCount = 256u;
}

FieldCPtr FieldInt8::nullField_ = FieldCPtr(new FieldInt8);

FieldInt8::FieldInt8(int8 value)
  : Field(ValueType::INT8, true)
{
  signedInteger_ = value;
}

FieldInt8::FieldInt8()
  : Field(ValueType::INT8, false)
{
}

FieldInt8::~FieldInt8()
{
}

int8
FieldInt8::toInt8() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return static_cast<int8>(signedInteger_);
}

FieldCPtr
FieldInt8::create(int8 value)
{
  static FieldCPtr entries[internCount];
  static const bool initialized = []
  {
    for(int v = -128; v <= 127; ++v)
    {
      entries[static_cast<size_t>(v + 128)].reset(
        new FieldInt8(static_cast<int8>(v)));
    }
    return true;
  }();
  (void)initialized;
  return entries[static_cast<size_t>(static_cast<int>(value) + 128)];
}

FieldCPtr
FieldInt8::createNull()
{
  return nullField_;
}

void
FieldInt8::valueToStringBuffer() const
{
  std::stringstream buffer;
  buffer << signedInteger_;
  string_.assign(reinterpret_cast<const unsigned char *>(buffer.str().data()), buffer.str().size());
}

bool
FieldInt8::isSignedInteger()const
{
  return true;
}
