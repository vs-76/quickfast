// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "Decimal.h"
#include <Common/Exceptions.h>
#include <Common/LexicalCast.h>
#include <Common/WorkingBuffer.h>
#include <cctype>

using namespace ::QuickFAST;

namespace {
  void trimInPlace(std::string & str)
  {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), notSpace));
    str.erase(std::find_if(str.rbegin(), str.rend(), notSpace).base(), str.end());
  }

  /// @brief Can this mantissa be multiplied by ten and still be a mantissa_t?
  ///
  /// Both bounds are needed: guarding only the positive side leaves every
  /// negative mantissa unbounded, which is how the multiply became undefined
  /// behaviour rather than a value the caller could notice.
  bool canScaleUp(mantissa_t mantissa)
  {
    return mantissa <= (LLONG_MAX / 10) && mantissa >= (LLONG_MIN / 10);
  }

  /// @brief Refuse an arithmetic operation whose operands share no exponent.
  ///
  /// Adding mantissas that stand for different powers of ten produces a
  /// number unrelated to either operand, which is worse than declining.
  void requireCommonExponent(bool reached)
  {
    if(!reached)
    {
      throw OverflowError(
        "[ERR R1]Decimal operands cannot be brought to a common exponent.");
    }
  }

  /// @brief Is this a plain, optionally signed run of decimal digits?
  ///
  /// parse() splices the fractional part onto the whole part, so "1.2.3"
  /// reaches the conversion as "12.3" -- malformed input arrives looking
  /// almost right, and used to throw out of a void function.
  bool isIntegerString(const std::string & str)
  {
    size_t pos = 0;
    if(pos < str.size() && (str[pos] == '-' || str[pos] == '+'))
    {
      ++pos;
    }
    if(pos == str.size())
    {
      return false;
    }
    for(; pos < str.size(); ++pos)
    {
      if(std::isdigit(static_cast<unsigned char>(str[pos])) == 0)
      {
        return false;
      }
    }
    return true;
  }

  /// @brief Narrow an exponent computed at full width.
  ///
  /// exponent_t is int8_t while normalize() permits 64, so a sum of two legal
  /// exponents need not be legal. The narrowing is well-defined modular
  /// conversion, so it wrapped silently: 64 + 64 became -128 and normalize
  /// then reported an *underflow* for what was an overflow.
  exponent_t checkedExponent(int exponent)
  {
    if(exponent > 64)
    {
      throw OverflowError("[ERR R1]Decimal Exponent overflow.");
    }
    if(exponent < -64)
    {
      throw OverflowError("[ERR R1]Decimal Exponent undeflow.");
    }
    return exponent_t(exponent);
  }
}

Decimal::Decimal(
    mantissa_t mantissa,
    exponent_t exponent,
    bool autoNormalize)
: mantissa_(mantissa)
, exponent_(exponent)
, autoNormalize_(autoNormalize)
{
}

Decimal::Decimal(const Decimal & rhs)
: mantissa_(rhs.mantissa_)
, exponent_(rhs.exponent_)
, autoNormalize_(rhs.autoNormalize_)
{
}

Decimal::~Decimal()
{
}

void
Decimal::parse(const std::string & value)
{
  std::string str = value;
  trimInPlace(str);
  size_t dotPos = str.find(".");
  std::string wholeString = str.substr(0,dotPos);
  std::string fracString;
  if(dotPos != std::string::npos)
  {
    fracString = str.substr(dotPos+1);
  }
  std::string mantissaString = wholeString+fracString;

  // Validate before converting. Only the first conversion below was guarded,
  // so input that was malformed rather than merely too large reached the
  // recovery conversion and threw bad_lexical_cast straight out of parse().
  if(!isIntegerString(mantissaString))
  {
    throw UsageError("Cannot parse decimal", value.c_str());
  }

  // fracString.size() is a size_t and exponent_ is int8_t: 200 fractional
  // digits narrowed to int8_t(200) == -56 and then negated to +56, so the
  // value came out wrong by about 10^112 with the sign of the exponent
  // inverted.
  if(fracString.size() > 64)
  {
    throw OverflowError("[ERR R1]Decimal Exponent undeflow.");
  }
  exponent_ = -exponent_t(fracString.size());

  bool overflow = false;

  // VC8 stringstream is truncating a large string instead of throwing
  // the exception, so we'll always fall back on the overflow code.
#if defined _MSC_VER && _MSC_VER < 1500
  overflow = true;
#else
  try {
    mantissa_ = QuickFAST::lexical_cast<mantissa_t>(mantissaString);
  }
  catch (std::exception &)
  {
    overflow = true;
  }
#endif

  if (overflow)
  {
    // Recovery is only possible by trimming trailing zeros, which needs
    // autonormalization to be wanted in the first place.
    const size_t pos = autoNormalize_
      ? mantissaString.find_last_not_of("0")
      : std::string::npos;
    if(pos == std::string::npos)
    {
      // All zeros, or nothing to trim: the mantissa genuinely does not fit.
      throw OverflowError("[ERR R1]Decimal mantissa overflow parsing " + value);
    }
    exponent_ = checkedExponent(
      int(exponent_) + int(mantissaString.length() - pos - 1));
    mantissaString = mantissaString.substr (0, pos + 1);
    try
    {
      mantissa_ = QuickFAST::lexical_cast<mantissa_t>(mantissaString);
    }
    catch (const std::exception &)
    {
      throw OverflowError("[ERR R1]Decimal mantissa overflow parsing " + value);
    }
  }

  if(autoNormalize_)
  {
    normalize();
  }
}

void
Decimal::setAutoNormalize(bool autoNormalize)
{
  autoNormalize_ = autoNormalize;
  if(autoNormalize_)
  {
    normalize();
  }
}

void
Decimal::setMantissa(mantissa_t mantissa)
{
  mantissa_ = mantissa;
}

void
Decimal::setExponent(exponent_t exponent)
{
  exponent_ = exponent;
}



mantissa_t
Decimal::getMantissa() const
{
  return mantissa_;
}

exponent_t
Decimal::getExponent() const
{
  return exponent_;
}

void
Decimal::toString(std::string & value)const
{
#if 0
  value = QuickFAST::lexical_cast<std::string>(double(*this));
#elif 1
  std::stringstream str;
  str << double(*this);
  value = str.str();
#else
  WorkingBuffer buffer;
  buffer.clear(true, 20);
  bool negative = false;
  int64 m = mantissa_;
  if(m < 0)
  {
    negative = true;
    m = -m;
  }
  short e = exponent_;
  if(e >= 0)
  {
    // No trailing decimal point
    // buffer.push((unsigned char)'.');
    while(e > 0)
    {
      buffer.push((unsigned char)'0');
      e -= 1;
    }
  }
  bool none = true;
  while(m != 0 || e < 0 || none)
  {
    none = false;
    char c = (m % 10) + '0';
    m /= 10;
    buffer.push((unsigned char)c);
    if(e < 0)
    {
      e += 1;
      if(e == 0)
      {
        buffer.push((unsigned char)'.');
        // insure at least one character to the left of the decimal point
        if(m == 0)
        {
          buffer.push((unsigned char)'0');
        }
      }
    }
  }
  if(negative)
  {
    buffer.push((unsigned char)'-');
  }
  value.assign((const char *)buffer.begin(), buffer.end()-buffer.begin());
#endif
}

Decimal &
Decimal::operator = (const Decimal & rhs)
{
  Decimal temp(rhs);
  swap(temp);
  return *this;
}

void
Decimal::swap(Decimal & rhs)
{
  exponent_t texp = exponent_;
  exponent_ = rhs.exponent_;
  rhs.exponent_ = texp;
  mantissa_t tman = mantissa_;
  mantissa_ = rhs.mantissa_;
  rhs.mantissa_ = tman;
  // operator= is copy-and-swap, so leaving this out made assignment produce a
  // different object than copy construction from the same source: the value
  // came from rhs while the flag stayed with the target.
  bool tnorm = autoNormalize_;
  autoNormalize_ = rhs.autoNormalize_;
  rhs.autoNormalize_ = tnorm;
}

void
Decimal::normalize(bool strict /*= true*/)
{
  while(mantissa_ != 0 && mantissa_ % 10 == 0 && exponent_ < 64)
  {
    mantissa_ /= 10;
    exponent_ += 1;
  }
  if(exponent_ > 64)
  {
    if(strict)
    {
      throw OverflowError("[ERR R1]Decimal Exponent overflow.");
    }
    // Scaling the mantissa up is a recovery only while it still fits. Past
    // that the multiply overflowed and the "recovered" value was nonsense, so
    // report the condition the non-strict path could not actually recover from.
    while(exponent_ > 64 && canScaleUp(mantissa_))
    {
      mantissa_ *= 10;
      exponent_ -= 1;
    }
    if(exponent_ > 64)
    {
      throw OverflowError("[ERR R1]Decimal Exponent overflow.");
    }
  }
  if(exponent_ < -64)
  {
    if(strict)
    {
      throw OverflowError("[ERR R1]Decimal Exponent undeflow.");
    }
    while(exponent_ < -64)
    {
      mantissa_ /= 10;
      exponent_ += 1;
    }
  }
}

bool
Decimal::denormalize(exponent_t exponent)
{
  while(exponent_ > exponent && canScaleUp(mantissa_))
  {
    exponent_ -= 1;
    mantissa_ *= 10;
  }
  return exponent_ == exponent;
}

void
Decimal::maximizeMantissa()
{
  // this could be considerably faster!
  while(exponent_ > SCHAR_MIN && canScaleUp(mantissa_))
  {
    exponent_ -= 1;
    mantissa_ *= 10;
  }
}

bool
Decimal::operator<(const Decimal & rhs) const
{
  if(exponent_ == rhs.exponent_)
  {
    return mantissa_ < rhs.mantissa_;
  }

  if(rhs.exponent_ < exponent_)
  {
    Decimal temp(*this);
    if(temp.denormalize(rhs.exponent_))
    {
      return temp.mantissa_ < rhs.mantissa_;
    }
  }
  else
  {
    Decimal temp(rhs);
    if(temp.denormalize(exponent_))
    {
      return mantissa_ < temp.mantissa_;
    }
  }
  // No common exponent fits in a mantissa_t, so the two cannot be compared as
  // integers at all.  Comparing the truncated mantissas is not an
  // approximation, it is a different question with a different answer:
  // 1e20 < 5e18 came out true.  double loses precision but keeps the ordering,
  // which is the part a comparison operator has to get right.
  return double(*this) < double(rhs);
}

bool
Decimal::operator==(const Decimal & rhs) const
{
  if(exponent_ == rhs.exponent_)
  {
    return mantissa_ == rhs.mantissa_;
  }

  if(rhs.exponent_ < exponent_)
  {
    Decimal temp(*this);
    if(temp.denormalize(rhs.exponent_))
    {
      return temp.mantissa_ == rhs.mantissa_;
    }
  }
  else
  {
    Decimal temp(rhs);
    if(temp.denormalize(exponent_))
    {
      return mantissa_== temp.mantissa_;
    }
  }
  // See operator<: without a common exponent the truncated mantissas can be
  // equal while the values are not, which reported 1e20 == 1e18.
  return double(*this) == double(rhs); //-V550
}

Decimal::operator double()const
{
  // faster than the pow function or repeated multiply/divides
  static double powerTable[] =
  {
      1.0e-63, 1.0e-62, 1.0e-61, 1.0e-60, 1.0e-59, 1.0e-58, 1.0e-57, 1.0e-56,
      1.0e-55, 1.0e-54, 1.0e-53, 1.0e-52, 1.0e-51, 1.0e-50, 1.0e-49, 1.0e-48,
      1.0e-47, 1.0e-46, 1.0e-45, 1.0e-44, 1.0e-43, 1.0e-42, 1.0e-41, 1.0e-40,
      1.0e-39, 1.0e-38, 1.0e-37, 1.0e-36, 1.0e-35, 1.0e-34, 1.0e-33, 1.0e-32,
      1.0e-31, 1.0e-30, 1.0e-29, 1.0e-28, 1.0e-27, 1.0e-26, 1.0e-25, 1.0e-24,
      1.0e-23, 1.0e-22, 1.0e-21, 1.0e-20, 1.0e-19, 1.0e-18, 1.0e-17, 1.0e-16,
      1.0e-15, 1.0e-14, 1.0e-13, 1.0e-12, 1.0e-11, 1.0e-10, 1.0e-9 , 1.0e-8 ,
      1.0e-7 , 1.0e-6 , 1.0e-5 , 1.0e-4 , 1.0e-3 , 1.0e-2 , 1.0e-1 , 1.0e0  ,
      1.0e1  , 1.0e2  , 1.0e3  , 1.0e4  , 1.0e5  , 1.0e6  , 1.0e7  , 1.0e8  ,
      1.0e9  , 1.0e10 , 1.0e11 , 1.0e12 , 1.0e13 , 1.0e14 , 1.0e15 , 1.0e16 ,
      1.0e17 , 1.0e18 , 1.0e19 , 1.0e20 , 1.0e21 , 1.0e22 , 1.0e23 , 1.0e24 ,
      1.0e25 , 1.0e26 , 1.0e27 , 1.0e28 , 1.0e29 , 1.0e30 , 1.0e31 , 1.0e32 ,
      1.0e33 , 1.0e34 , 1.0e35 , 1.0e36 , 1.0e37 , 1.0e38 , 1.0e39 , 1.0e40 ,
      1.0e41 , 1.0e42 , 1.0e43 , 1.0e44 , 1.0e45 , 1.0e46 , 1.0e47 , 1.0e48 ,
      1.0e49 , 1.0e50 , 1.0e51 , 1.0e52 , 1.0e53 , 1.0e54 , 1.0e55 , 1.0e56 ,
      1.0e57 , 1.0e58 , 1.0e59 , 1.0e60 , 1.0e61 , 1.0e62 , 1.0e63
  };
  if(exponent_ >= -63 && exponent_ <= 63)
  {
    return (double)mantissa_ * powerTable[exponent_ + 63];
  }
  // note it is a FAST reportable error if abs(exponent) > 63, but for now we'll just fake it here.
  return double(double(mantissa_) * pow(10.0L, exponent_));
}

Decimal &
Decimal::operator+=(const Decimal & rhs)
{
  if(rhs.exponent_ < exponent_)
  {
    Decimal temp(*this);
    requireCommonExponent(temp.denormalize(rhs.exponent_));
    temp.mantissa_ += rhs.mantissa_;
    temp.normalize();
    swap(temp);
  }
  else
  {
    Decimal temp(rhs);
    requireCommonExponent(temp.denormalize(exponent_));
    temp.mantissa_ += mantissa_;
    temp.normalize();
    swap(temp);
  }
  return *this;
}


Decimal &
Decimal::operator-=(const Decimal & rhs)
{
  if(rhs.exponent_ < exponent_)
  {
    Decimal temp(*this);
    requireCommonExponent(temp.denormalize(rhs.exponent_));
    temp.mantissa_ -= rhs.mantissa_;
    temp.normalize();
    swap(temp);
  }
  else
  {
    Decimal temp(rhs);
    requireCommonExponent(temp.denormalize(exponent_));
    temp.mantissa_ = mantissa_ - temp.mantissa_;
    temp.normalize();
    swap(temp);
  }
  return *this;
}

Decimal &
Decimal::operator*=(const Decimal & rhs)
{
  Decimal temp(*this);
  temp.exponent_ = checkedExponent(int(exponent_) + int(rhs.exponent_));
  temp.mantissa_ *= rhs.mantissa_;
  temp.normalize();
  swap(temp);
  return *this;
}

Decimal&
Decimal::operator/=(const Decimal & rhs)
{
  if(rhs.mantissa_ == 0)
  {
    // A default-constructed Decimal has a zero mantissa, so this was reachable
    // by dividing by one: SIGFPE rather than something a caller can handle.
    throw OverflowError("[ERR R1]Decimal division by zero.");
  }
  Decimal temp(*this);
  temp.maximizeMantissa();
  // normalize(false) below pulls an out-of-range exponent back, so only the
  // int8_t narrowing itself has to be guarded here.
  const int exponent = int(temp.exponent_) - int(rhs.exponent_);
  if(exponent > SCHAR_MAX || exponent < SCHAR_MIN)
  {
    throw OverflowError("[ERR R1]Decimal Exponent overflow.");
  }
  temp.exponent_ = exponent_t(exponent);
  if(temp.mantissa_ == LLONG_MIN && rhs.mantissa_ == -1)
  {
    throw OverflowError("[ERR R1]Decimal mantissa overflow on division.");
  }
  temp.mantissa_ /= rhs.mantissa_;
  temp.normalize(false);
  swap(temp);
  return *this;
}
