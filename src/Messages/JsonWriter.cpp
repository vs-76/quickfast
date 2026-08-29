// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//

#include <Common/QuickFASTPch.h>
#include "JsonWriter.h"
#include <ostream>
#include <sstream>

using namespace QuickFAST;
using namespace Messages;

JsonWriter::JsonWriter(std::ostream & out)
  : out_(out)
  , expectValue_(false)
{
}

JsonWriter::~JsonWriter()
{
}

void
JsonWriter::separate()
{
  if(expectValue_)
  {
    expectValue_ = false;
    return;
  }
  if(!needComma_.empty() && needComma_.back())
  {
    out_ << ',';
  }
  if(!needComma_.empty())
  {
    needComma_.back() = true;
  }
}

void
JsonWriter::beginObject()
{
  separate();
  out_ << '{';
  needComma_.push_back(false);
}

void
JsonWriter::endObject()
{
  out_ << '}';
  if(!needComma_.empty())
  {
    needComma_.pop_back();
  }
}

void
JsonWriter::beginArray()
{
  separate();
  out_ << '[';
  needComma_.push_back(false);
}

void
JsonWriter::endArray()
{
  out_ << ']';
  if(!needComma_.empty())
  {
    needComma_.pop_back();
  }
}

void
JsonWriter::writeKey(const std::string & key)
{
  separate();
  writeQuoted(key.data(), key.size());
  out_ << ':';
  expectValue_ = true;
}

void
JsonWriter::writeNull()
{
  separate();
  out_ << "null";
}

void
JsonWriter::writeBool(bool value)
{
  separate();
  out_ << (value ? "true" : "false");
}

void
JsonWriter::writeInt64(int64 value)
{
  separate();
  out_ << value;
}

void
JsonWriter::writeUInt32(uint32 value)
{
  separate();
  out_ << value;
}

void
JsonWriter::writeUInt64AsString(uint64 value)
{
  std::ostringstream oss;
  oss << value;
  writeString(oss.str());
}

void
JsonWriter::writeString(const std::string & value)
{
  writeString(value.data(), value.size());
}

void
JsonWriter::writeString(const char * value, size_t length)
{
  separate();
  writeQuoted(value, length);
}

void
JsonWriter::writeQuoted(const char * value, size_t length)
{
  out_ << '"';
  for(size_t i = 0; i < length; ++i)
  {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    switch(ch)
    {
    case '"':
      out_ << "\\\"";
      break;
    case '\\':
      out_ << "\\\\";
      break;
    case '\b':
      out_ << "\\b";
      break;
    case '\f':
      out_ << "\\f";
      break;
    case '\n':
      out_ << "\\n";
      break;
    case '\r':
      out_ << "\\r";
      break;
    case '\t':
      out_ << "\\t";
      break;
    default:
      if(ch < 0x20)
      {
        static const char hexDigits[] = "0123456789abcdef";
        out_ << "\\u00"
             << hexDigits[(ch >> 4) & 0x0f]
             << hexDigits[ch & 0x0f];
      }
      else
      {
        out_ << static_cast<char>(ch);
      }
      break;
    }
  }
  out_ << '"';
}
