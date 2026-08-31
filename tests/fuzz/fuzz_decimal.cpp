// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// libFuzzer entry for Decimal arithmetic and ordering consistency.
#include <Common/QuickFASTPch.h>

#include <Common/Decimal.h>

#include <cstdint>
#include <cstring>
#include <exception>

namespace {

QuickFAST::mantissa_t readI64(const uint8_t * p)
{
  QuickFAST::mantissa_t value = 0;
  std::memcpy(&value, p, sizeof(value));
  return value;
}

QuickFAST::exponent_t clampExponent(int8_t raw)
{
  // FAST allows roughly [-63, 63]; keep the harness inside that band so the
  // fuzzer spends time on arithmetic rather than constructor rejects alone.
  if(raw > 63)
  {
    return 63;
  }
  if(raw < -63)
  {
    return -63;
  }
  return raw;
}

void checkOrdering(
  const QuickFAST::Decimal & a,
  const QuickFAST::Decimal & b)
{
  const bool lt = a < b;
  const bool gt = b < a;
  const bool eq = a == b;
  // Trichotomy: exactly one of <, >, == must hold for a total order.
  // Decimal is a total order for finite values when comparisons succeed.
  if(static_cast<int>(lt) + static_cast<int>(gt) + static_cast<int>(eq) != 1)
  {
    __builtin_trap();
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  // Layout: mantA(8) expA(1) autoA(1) mantB(8) expB(1) autoB(1) op(1) = 21
  if(size < 21)
  {
    return 0;
  }

  const auto mantA = readI64(data);
  const auto expA = clampExponent(static_cast<int8_t>(data[8]));
  const bool autoA = (data[9] & 1) != 0;
  const auto mantB = readI64(data + 10);
  const auto expB = clampExponent(static_cast<int8_t>(data[18]));
  const bool autoB = (data[19] & 1) != 0;
  const uint8_t op = data[20] % 5;

  try
  {
    QuickFAST::Decimal a(mantA, expA, autoA);
    QuickFAST::Decimal b(mantB, expB, autoB);

    checkOrdering(a, b);

    QuickFAST::Decimal copy(a);
    (void)copy.denormalize(expB);
    a.normalize(false);

    switch(op)
    {
    case 0:
      a += b;
      break;
    case 1:
      a -= b;
      break;
    case 2:
      a *= b;
      break;
    case 3:
      if(b.getMantissa() != 0)
      {
        a /= b;
      }
      break;
    default:
      (void)(a + b);
      break;
    }
  }
  catch(const std::exception &)
  {
  }
  catch(...)
  {
  }
  return 0;
}
