// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef DECIMAL_H
#define DECIMAL_H
#include <Common/QuickFAST_Export.h>
#include <Common/Decimal_fwd.h>
#include <Common/Types.h>

namespace QuickFAST{
  /// @brief A scaled Decimal data type as specified in the FAST standard
  ///
  /// Values are represented as a base 10 exponent (-64 < exponent < 64)
  /// Values may be normalized or unnormalized.  The autonormalize flag
  /// determines whether normalization happens automatically after every operation.
  /// In a normalized value, the mantissa has no trailing zeros (base 10)
  ///
  /// Mantissa and exponent are kept separate rather than collapsed into a
  /// binary floating point number, so a price such as 31.42 stays exact.
  ///
  /// @par Example
  /// @code
  /// QuickFAST::Decimal price(3142, -2);   // 3142 * 10^-2 == 31.42
  ///
  /// std::string text;
  /// price.toString(text);                 // "31.42"
  ///
  /// price += QuickFAST::Decimal(8, -2);   // 31.5
  /// const double approximate = price;     // only where precision loss is acceptable
  ///
  /// // Match a wire format that wants a fixed exponent:
  /// QuickFAST::Decimal scaled(price);
  /// if(scaled.denormalize(-4))
  /// {
  ///   const QuickFAST::mantissa_t mantissa = scaled.getMantissa(); // 315000
  /// }
  /// @endcode
  class QuickFAST_Export Decimal
  {
  public:
    /// @brief Construct a Decimal, defaulting to 0.0 autonormalized
    explicit Decimal(
      mantissa_t mantissa = 0,
      exponent_t exponent = 0,
      bool autoNormalize = true);
    /// @brief Copy construct a decimal
    Decimal(const Decimal & rhs);
    /// @brief Destruct a decimal
    ~Decimal();
    /// @brief Parse a decimal value from a string
    ///
    /// Supports only www.fff format. [no explicit exponent]
    void parse(const std::string & value);
    /// @brief Set the autonormalize flag.
    void setAutoNormalize(bool autoNormalize);
    /// @brief Set the mantissa directly
    void setMantissa(mantissa_t mantissa);
    /// @brief Set the exponent directly (multiplies or divides by a power of 10)
    void setExponent(exponent_t exponent);
    /// @brief Get the mantissa value directly
    mantissa_t getMantissa() const;
    /// @brief Get the exponent value directly
    exponent_t getExponent() const;

    /// @brief Convert the value to a double, with potential loss of precision
    operator double()const;

    /// @brief Convert the value to an www.ffff formatted string
    void toString(std::string & value)const;

    /// @brief Assignment
    Decimal & operator=(const Decimal & rhs);

    /// @brief less than comparison
    bool operator<(const Decimal & rhs) const;
    /// @brief equality comparison
    bool operator==(const Decimal & rhs) const;

    /// @brief in place addition
    Decimal& operator+=(const Decimal & rhs);
    /// @brief in place subtraction
    Decimal& operator-=(const Decimal & rhs);
    /// @brief in place multiplication
    Decimal& operator*=(const Decimal & rhs);
    /// @brief in place division
    Decimal& operator/=(const Decimal & rhs);

    /// @brief force normalization
    ///
    /// @param strict if true throw if normalization fails; else leave the number accurate but denormalized
    void normalize(bool strict = true);

    /// @brief denormalize to achieve a specific exponent
    ///
    /// Normalization will happen even if it means a loss of precision.
    /// The target is not always reachable: scaling the mantissa up by ten
    /// eventually overflows, and this stops rather than overflowing.
    /// @param exponent is the desired exponent
    /// @returns true if the requested exponent was reached. A false return
    ///          means the mantissa is no longer comparable with one at the
    ///          requested exponent.
    bool denormalize(exponent_t exponent);

    /// @brief Nothrow; no allocate; constant time swap values.
    /// @param rhs is the Decimal with which to swap
    void swap(Decimal & rhs);

  private:
    void maximizeMantissa();

  private:
    mantissa_t mantissa_;
    exponent_t exponent_;
    bool autoNormalize_;
  };

  /// @brief inequality comparison
  /// @param lhs the left operand
  /// @param rhs the right operand
  /// @returns true unless the values compare equal
  inline bool operator!=(const Decimal & lhs, const Decimal & rhs)
  {
    return !(lhs == rhs);
  }
  /// @brief greater than comparison
  /// @param lhs the left operand
  /// @param rhs the right operand
  /// @returns true if lhs is greater than rhs
  inline bool operator>(const Decimal & lhs, const Decimal & rhs)
  {
    return rhs < lhs;
  }
  /// @brief less than or equal comparison
  /// @param lhs the left operand
  /// @param rhs the right operand
  /// @returns true unless lhs is greater than rhs
  inline bool operator<=(const Decimal & lhs, const Decimal & rhs)
  {
    return !(rhs < lhs);
  }
  /// @brief greater than or equal comparison
  /// @param lhs the left operand
  /// @param rhs the right operand
  /// @returns true unless lhs is less than rhs
  inline bool operator>=(const Decimal & lhs, const Decimal & rhs)
  {
    return !(lhs < rhs);
  }
  /// @brief addition
  /// @param lhs the left operand, taken by value and used to hold the result
  /// @param rhs the right operand
  /// @returns the sum
  inline Decimal operator+(Decimal lhs, const Decimal & rhs)
  {
    lhs += rhs;
    return lhs;
  }
  /// @brief subtraction
  /// @param lhs the left operand, taken by value and used to hold the result
  /// @param rhs the right operand
  /// @returns the difference
  inline Decimal operator-(Decimal lhs, const Decimal & rhs)
  {
    lhs -= rhs;
    return lhs;
  }
  /// @brief multiplication
  /// @param lhs the left operand, taken by value and used to hold the result
  /// @param rhs the right operand
  /// @returns the product
  inline Decimal operator*(Decimal lhs, const Decimal & rhs)
  {
    lhs *= rhs;
    return lhs;
  }
  /// @brief division
  /// @param lhs the left operand, taken by value and used to hold the result
  /// @param rhs the right operand
  /// @returns the quotient
  inline Decimal operator/(Decimal lhs, const Decimal & rhs)
  {
    lhs /= rhs;
    return lhs;
  }
}
#endif // DECIMAL_H
