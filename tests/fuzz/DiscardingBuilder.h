// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#pragma once

#include <Messages/ValueMessageBuilder.h>

namespace QuickFAST {
namespace Fuzz {

/// @brief ValueMessageBuilder that stores nothing.
///
/// Fuzz harnesses only care about crashes and sanitizer findings; allocating
/// decoded fields would amplify RSS without improving coverage.
class DiscardingBuilder : public Messages::ValueMessageBuilder
{
public:
  const std::string & getApplicationType() const override
  {
    static const std::string type("fuzz");
    return type;
  }

  const std::string & getApplicationTypeNs() const override
  {
    static const std::string ns;
    return ns;
  }

  void addValue(const Messages::FieldIdentity &, ValueType::Type, const int64) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint64) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const int32) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint32) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const int16) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const uint16) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const int8) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const uchar) override {}
  void addValue(const Messages::FieldIdentity &, ValueType::Type, const Decimal &) override {}
  void addValue(
    const Messages::FieldIdentity &, ValueType::Type, const unsigned char *, size_t) override {}

  ValueMessageBuilder & startMessage(
    const std::string &, const std::string &, size_t) override
  {
    return *this;
  }

  bool endMessage(ValueMessageBuilder &) override
  {
    return true;
  }

  bool ignoreMessage(ValueMessageBuilder &) override
  {
    return true;
  }

  ValueMessageBuilder & startSequence(
    const Messages::FieldIdentity &,
    const std::string &,
    const std::string &,
    size_t,
    const Messages::FieldIdentity &,
    size_t) override
  {
    return *this;
  }

  void endSequence(const Messages::FieldIdentity &, ValueMessageBuilder &) override {}

  ValueMessageBuilder & startSequenceEntry(
    const std::string &, const std::string &, size_t) override
  {
    return *this;
  }

  void endSequenceEntry(ValueMessageBuilder &) override {}

  ValueMessageBuilder & startGroup(
    const Messages::FieldIdentity &,
    const std::string &,
    const std::string &,
    size_t) override
  {
    return *this;
  }

  void endGroup(const Messages::FieldIdentity &, ValueMessageBuilder &) override {}

  bool wantLog(unsigned short) override
  {
    return false;
  }

  bool logMessage(unsigned short, const std::string &) override
  {
    return true;
  }

  bool reportDecodingError(const std::string &) override
  {
    return false;
  }

  bool reportCommunicationError(const std::string &) override
  {
    return false;
  }
};

} // namespace Fuzz
} // namespace QuickFAST
