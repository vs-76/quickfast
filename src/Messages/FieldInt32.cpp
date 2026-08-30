// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "FieldInt32.h"
#include <Common/Exceptions.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

namespace
{
  const int32 internMin = -128;
  const int32 internMax = 255;
  const size_t internCount = static_cast<size_t>(internMax - internMin + 1);
}

FieldCPtr FieldInt32::nullField_ = FieldCPtr(new FieldInt32);

FieldInt32::FieldInt32(int32 value)
  : Field(ValueType::INT32, true)
{
  signedInteger_ = value;
}

FieldInt32::FieldInt32()
  : Field(ValueType::INT32, false)
{
}

FieldInt32::~FieldInt32()
{
}

int32
FieldInt32::toInt32() const
{
  if(!valid_)
  {
    throw FieldNotPresent("Field not present");
  }
  return static_cast<int32>(signedInteger_);
}

FieldCPtr
FieldInt32::create(int32 value)
{
  static FieldCPtr entries[internCount];
  static const bool initialized = []
  {
    for(int32 v = internMin; v <= internMax; ++v)
    {
      entries[static_cast<size_t>(v - internMin)].reset(new FieldInt32(v));
    }
    return true;
  }();
  (void)initialized;

  if(value >= internMin && value <= internMax)
  {
    return entries[static_cast<size_t>(value - internMin)];
  }
#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
  Field::noteHeapCreate();
#endif
  return FieldCPtr(new FieldInt32(value));
}

FieldCPtr
FieldInt32::createNull()
{
  return nullField_;
}

void
FieldInt32::valueToStringBuffer() const
{
  std::stringstream buffer;
  buffer << signedInteger_;
  string_.assign(reinterpret_cast<const unsigned char *>(buffer.str().data()), buffer.str().size());
}

bool
FieldInt32::isSignedInteger()const
{
  return true;
}
