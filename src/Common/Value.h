// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef VALUE_H
#define VALUE_H
#include "Value_fwd.h"
#include <Common/Types.h>
#include <Common/Exceptions.h>
#include <Common/Decimal.h>
#include <Common/StringBuffer.h>
#include <memory>
namespace QuickFAST{

  /// @brief A container for several different types of values
  ///
  /// Numeric / empty / undefined entries do not allocate a StringBuffer.
  /// String payloads and displayString() caches allocate one on demand so
  /// dictionary arrays of integers stay dense.
  ///
  /// Once allocated the buffer is kept: erase() and numeric assignment clear
  /// it rather than release it. Context::reset() erases every dictionary entry
  /// between messages, and freeing there would cost a free/malloc pair per
  /// string entry per message.
  ///
  class Value
  {
  public:
    /// @brief Enumerate the types of values (orable)
    enum ValueClass
    {
      SIGNEDINTEGER = 2,
      UNSIGNEDINTEGER = 4,
      DECIMAL = 8,
      STRING = 16,
      COMPOUND = 32,  // No data is stored for compound value types
      EMPTY = 64,
      UNDEFINED = 1
    };

    Value()
      : class_(UNDEFINED)
      , cachedString_(false)
      , unsignedInteger_(0)
      , signedInteger_(0)
      , exponent_(0)
    {
    }

    /// @brief Copy construct, duplicating any string storage.
    /// @param rhs is the value to copy
    Value(const Value & rhs)
      : class_(rhs.class_)
      , cachedString_(rhs.cachedString_)
      , unsignedInteger_(rhs.unsignedInteger_)
      , signedInteger_(rhs.signedInteger_)
      , exponent_(rhs.exponent_)
      , string_(rhs.string_ ? std::make_unique<StringBuffer>(*rhs.string_) : nullptr)
    {
    }

    /// @brief Move construct, stealing any string storage.
    ///
    /// The moved-from value keeps its class but loses its string buffer, so
    /// isString() may report true while getValue() returns false. Assign or
    /// erase() it before reading it again.
    Value(Value &&) noexcept = default;

    /// @brief a typical destructor.
    ~Value() = default;

    /// @brief Copy assign, reusing this value's string buffer when it has one.
    /// @param rhs is the value to copy
    /// @returns a reference to this value
    Value & operator=(const Value & rhs)
    {
      if(this != &rhs)
      {
        class_ = rhs.class_;
        cachedString_ = rhs.cachedString_;
        unsignedInteger_ = rhs.unsignedInteger_;
        signedInteger_ = rhs.signedInteger_;
        exponent_ = rhs.exponent_;
        if(rhs.string_)
        {
          if(string_)
          {
            *string_ = *rhs.string_;
          }
          else
          {
            string_ = std::make_unique<StringBuffer>(*rhs.string_);
          }
        }
        else
        {
          string_.reset();
        }
      }
      return *this;
    }

    /// @brief Move assign, stealing any string storage.
    ///
    /// Leaves the source in the state described by the move constructor.
    /// @returns a reference to this value
    Value & operator=(Value &&) noexcept = default;

    /// @brief Set Compound type
    void setCompound()
    {
      class_ = COMPOUND;
      discardStringStorage();
    }

    /// @brief reset the value class
    /// @param undefined defaults to UNDEFINED but can be used to set other classes for testing
    ///
    void setUndefined(ValueClass undefined = UNDEFINED)
    {
      class_ = undefined;
      discardStringStorage();
    }

    /// @brief set the value to NULL
    void setNull()
    {
      class_ = EMPTY;
      discardStringStorage();
    }

    /// @brief check for NULL value
    bool isNull() const
    {
      return ((class_ & EMPTY) == EMPTY);
    }

    /// @brief set the value to undefined
    void erase()
    {
      class_ = UNDEFINED;
      discardStringStorage();
      signedInteger_ = 0;
      unsignedInteger_ = 0;
      exponent_ = 0;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const int64 value)
    {
      class_ = SIGNEDINTEGER;
      discardStringStorage();
      signedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const uint64 value)
    {
      class_ = UNSIGNEDINTEGER;
      discardStringStorage();
      unsignedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const int32 value)
    {
      class_ = SIGNEDINTEGER;
      discardStringStorage();
      signedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const uint32 value)
    {
      class_ = UNSIGNEDINTEGER;
      discardStringStorage();
      unsignedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const int16 value)
    {
      class_ = SIGNEDINTEGER;
      discardStringStorage();
      signedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const uint16 value)
    {
      class_ = UNSIGNEDINTEGER;
      discardStringStorage();
      unsignedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const int8 value)
    {
      class_ = SIGNEDINTEGER;
      discardStringStorage();
      signedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const uchar value)
    {
      class_ = UNSIGNEDINTEGER;
      discardStringStorage();
      unsignedInteger_ = value;
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const Decimal& value)
    {
      class_ = DECIMAL;
      discardStringStorage();
      exponent_ = value.getExponent();
      signedInteger_ = value.getMantissa();
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    /// @param length is the lenght of the string pointed to by value.
    void setValue(const unsigned char * value, size_t length)
    {
      class_ = STRING;
      cachedString_ = true;
      ensureString().assign(value, length);
    }

    /// @brief assign a value from a null terminated C-style string
    ///
    /// @param value is the value to be assigned
    void setValue(const char * value)
    {
      setValue(reinterpret_cast<const unsigned char*>(value), std::strlen(value));
    }

    /// @brief assign a value
    /// @param value is the value to be assigned
    void setValue(const std::string& value)
    {
      setValue(reinterpret_cast<const unsigned char*>(value.c_str()), value.length());
    }

    /// @brief check for class and value equality
    bool operator == (const Value & rhs) const
    {
      if(((class_ | rhs.class_) & UNDEFINED) == UNDEFINED)
      {
        return false;
      }
      if(class_ != rhs.class_)
      {
        return false;
      }
      if(unsignedInteger_ != rhs.unsignedInteger_ ||
        signedInteger_ != rhs.signedInteger_ ||
        exponent_ != rhs.exponent_)
      {
        return false;
      }
      if((class_ & STRING) == STRING)
      {
        if(!string_ || !rhs.string_)
        {
          return string_ == rhs.string_;
        }
        return *string_ == *rhs.string_;
      }
      return true;
    }

    /// @brief inequality operator
    bool operator != (const Value & rhs)const
    {
      return ! (*this == rhs);
    }

    /// @brief display the value as a string.  Low performance
    const StringBuffer & displayString() const
    {
      if(isDefined() && !cachedString_)
      {
        valueToStringBuffer();
      }
      return ensureString();
    }

    /// @brief Does this field have a value?
    /// @return true if the field has a value.
    bool isDefined() const
    {
      return (class_ & UNDEFINED) == 0;
    }

    /// @brief Is this field a kind of string (Ascii, Utf8, or ByteVector)?
    bool isString()const
    {
      return (class_ & (UNDEFINED | STRING)) == STRING;
    }

    /// @brief true if signed integer
    bool isSignedInteger() const
    {
      return (class_ & (UNDEFINED | SIGNEDINTEGER)) == SIGNEDINTEGER;
    }

    /// @brief true if unsigned integer
    bool isUnsignedInteger() const
    {
      return (class_ & (UNDEFINED | UNSIGNEDINTEGER)) == UNSIGNEDINTEGER;
    }

    /// @brief true if numberic (integer, unsigned integer or decimal)
    bool isNumeric() const
    {
      return (class_ & (UNDEFINED | STRING | COMPOUND)) == 0;
    }

    /// @brief true if compound value
    bool isCompound() const
    {
      return (class_ & (UNDEFINED | COMPOUND)) == COMPOUND;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(int64 & value) const
    {
      if(class_ == SIGNEDINTEGER)
      {
        value = signedInteger_;
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(uint64 & value) const
    {
      if(class_ == UNSIGNEDINTEGER)
      {
        value = unsignedInteger_;
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(int32 & value) const
    {
      if(class_ == SIGNEDINTEGER)
      {
        value = static_cast<int32>(signedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(uint32 & value) const
    {
      if(class_ == UNSIGNEDINTEGER)
      {
        value = static_cast<uint32>(unsignedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(int16 & value) const
    {
      if(class_ == SIGNEDINTEGER)
      {
        value = static_cast<int16>(signedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(uint16 & value) const
    {
      if(class_ == UNSIGNEDINTEGER)
      {
        value = static_cast<uint16>(unsignedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(int8 & value) const
    {
      if(class_ == SIGNEDINTEGER)
      {
        value = static_cast<int8>(signedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(uchar & value) const
    {
      if(class_ == UNSIGNEDINTEGER)
      {
        value = static_cast<uchar>(unsignedInteger_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(Decimal & value) const
    {
      if(class_ == DECIMAL)
      {
        value = Decimal(signedInteger_, exponent_);
        return true;
      }
      return false;

    }

    /// @brief get the value
    /// @param value is set to point to the string
    /// @param length is the length of the string.
    bool getValue(const unsigned char *& value, size_t &length) const
    {
      if(class_ == STRING && string_)
      {
        value = string_->data();
        length = string_->size();
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value is set to point to the string
    bool getValue(const char *& value) const
    {
      if(class_ == STRING && string_)
      {
        value = reinterpret_cast<const char *>(string_->c_str());
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @param value receives the data
    bool getValue(std::string& value) const
    {
      if(class_ == STRING && string_)
      {
        value = static_cast<std::string>(*string_);
        return true;
      }
      return false;
    }

    /// @brief get the value
    /// @returns the value
    uint64 getUnsignedInteger()const
    {
      if(class_ != UNSIGNEDINTEGER)
      {
        UnsupportedConversion ex("Value is not unsigned integer.");
        throw ex;
      }
      return unsignedInteger_;
    }

    /// @brief get the value
    /// @returns the value
    int64 getSignedInteger()const
    {
      if(class_ != SIGNEDINTEGER)
      {
        UnsupportedConversion ex("Value is not signed integer.");
        throw ex;
      }
      return signedInteger_;
    }

    /// @brief get the value
    /// @returns the value
    int64 getMantissa()const
    {
      if(class_ != DECIMAL)
      {
        UnsupportedConversion ex("Value is not decimal.");
        throw ex;
      }
      return signedInteger_;
    }

    /// @brief get the value
    /// @returns the value
    exponent_t getExponent()const
    {
      if(class_ != DECIMAL)
      {
        UnsupportedConversion ex("Value is not decimal.");
        throw ex;
      }
      return exponent_;
    }

    /// @brief get the value
    /// @returns the value
    Decimal getDecimal()const
    {
      if(class_ != DECIMAL)
      {
        UnsupportedConversion ex("Value is not decimal.");
        throw ex;
      }
      return Decimal(signedInteger_, exponent_);
    }

  private:
    /// @brief Invalidate the string payload / display cache.
    ///
    /// Keeps any buffer already allocated. Context::reset() erases every
    /// dictionary entry between messages, so freeing here would cost a
    /// free/malloc pair per string-valued entry per message. Slots that never
    /// hold a string still never allocate, which is where the density comes
    /// from.
    void discardStringStorage() const
    {
      cachedString_ = false;
      if(string_)
      {
        string_->erase();
      }
    }

    /// @brief Allocate string storage if needed and return it.
    StringBuffer & ensureString() const
    {
      if(!string_)
      {
        string_ = std::make_unique<StringBuffer>();
      }
      return *string_;
    }

    /// @brief Produce a cached "human readable" representation of the value
    void valueToStringBuffer()const
    {
      if(cachedString_ || (class_ == STRING))
      {
        return;
      }
      std::stringstream buffer;
      if(class_ == SIGNEDINTEGER)
      {
        buffer << signedInteger_;
      }
      else if(class_ == UNSIGNEDINTEGER)
      {
        buffer << unsignedInteger_;
      }
      else if(class_ == DECIMAL)
      {
        Decimal d(signedInteger_, exponent_);

        buffer << static_cast<double>(d);
      }
      else if(class_ == EMPTY)
      {
        buffer << "[null]";
      }
      ensureString() = buffer.str();
    }


  private:
    /// What class of information is this Value
    /// Bits are ORed.
    ValueClass class_;

    /// @brief true if string representation of the value has been cached
    mutable bool cachedString_;

    ///////////////////////////////////////////////////
    // Value contents

    ///@brief Data for any of the unsigned integral types.
    unsigned long long unsignedInteger_;

    ///@brief Data for any of the signed integral types. Also Decimal mantissa.
    signed long long signedInteger_;

    ///@brief Exponent for Decimal types (mantissa is in signedInteger_)
    exponent_t exponent_;

    ///@brief On-demand buffer for STRING values and displayString() caches.
    mutable std::unique_ptr<StringBuffer> string_;
  };
}

#endif // VALUE_H
