// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//

#include <Common/QuickFASTPch.h>
#include "MessageToJson.h"
#include "JsonWriter.h"
#include <Messages/Message.h>
#include <Messages/FieldIdentity.h>
#include <Messages/Sequence.h>
#include <Messages/Group.h>
#include <Common/Decimal.h>
#include <Common/StringBuffer.h>
#include <algorithm>
#include <limits>
#include <ostream>

using namespace QuickFAST;
using namespace Messages;

namespace
{
  const char base64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

MessageToJson::MessageToJson(const JsonOptions & options)
  : options_(options)
{
}

MessageToJson::~MessageToJson()
{
}

void
MessageToJson::formatMessage(const Message & message, std::ostream & out) const
{
  formatFieldSet(message, out);
}

void
MessageToJson::formatFieldSet(const FieldSet & fields, std::ostream & out) const
{
  JsonWriter writer(out);
  writer.beginObject();
  writeFieldSet(fields, writer);
  writer.endObject();
}

void
MessageToJson::writeFieldSet(const FieldSet & fields, JsonWriter & writer) const
{
  if(options_.includeApplicationType && !fields.getApplicationType().empty())
  {
    writer.writeKey("applicationType");
    writer.writeString(fields.getApplicationType());
    if(!fields.getApplicationTypeNs().empty())
    {
      writer.writeKey("applicationTypeNs");
      writer.writeString(fields.getApplicationTypeNs());
    }
  }

  for(FieldSet::const_iterator it = fields.begin(); it != fields.end(); ++it)
  {
    writeField(it->getIdentity(), it->getField(), writer);
  }
}

void
MessageToJson::writeField(
  const FieldIdentity & identity,
  const FieldCPtr & field,
  JsonWriter & writer) const
{
  if(!field || !field->isDefined())
  {
    return;
  }

  const ValueType::Type type = field->getType();
  if(type == ValueType::SEQUENCE)
  {
    writeSequence(identity, field, writer);
  }
  else if(type == ValueType::GROUP)
  {
    writeGroup(identity, field, writer);
  }
  else
  {
    writeScalar(identity, field, writer);
  }
}

void
MessageToJson::writeSequence(
  const FieldIdentity & identity,
  const FieldCPtr & field,
  JsonWriter & writer) const
{
  writer.writeKey(fieldKey(identity));
  writer.beginArray();
  SequenceCPtr sequence = field->toSequence();
  for(Sequence::const_iterator it = sequence->begin();
    it != sequence->end();
    ++it)
  {
    writer.beginObject();
    writeFieldSet(**it, writer);
    writer.endObject();
  }
  writer.endArray();
}

void
MessageToJson::writeGroup(
  const FieldIdentity & identity,
  const FieldCPtr & field,
  JsonWriter & writer) const
{
  writer.writeKey(fieldKey(identity));
  writer.beginObject();
  writeFieldSet(*field->toGroup(), writer);
  writer.endObject();
}

namespace
{
  /// Exact decimal text from mantissa/exponent (avoids Decimal::toString's double path).
  std::string formatDecimalExact(const Decimal & decimal)
  {
    const mantissa_t mantissa = decimal.getMantissa();
    exponent_t exponent = decimal.getExponent();

    const bool negative = mantissa < 0;
    uint64 absolute = 0;
    if(mantissa == std::numeric_limits<mantissa_t>::min())
    {
      absolute = static_cast<uint64>(-(mantissa + 1)) + 1u;
    }
    else if(negative)
    {
      absolute = static_cast<uint64>(-mantissa);
    }
    else
    {
      absolute = static_cast<uint64>(mantissa);
    }

    std::string digits;
    if(absolute == 0)
    {
      digits = "0";
    }
    else
    {
      while(absolute != 0)
      {
        digits.push_back(static_cast<char>('0' + (absolute % 10)));
        absolute /= 10;
      }
      std::reverse(digits.begin(), digits.end());
    }

    std::string result;
    if(negative)
    {
      result.push_back('-');
    }

    if(exponent >= 0)
    {
      result += digits;
      result.append(static_cast<size_t>(exponent), '0');
      return result;
    }

    const int point = static_cast<int>(digits.size()) + exponent;
    if(point <= 0)
    {
      result += "0.";
      result.append(static_cast<size_t>(-point), '0');
      result += digits;
    }
    else
    {
      result.append(digits, 0, static_cast<size_t>(point));
      result.push_back('.');
      result.append(digits, static_cast<size_t>(point), std::string::npos);
    }
    return result;
  }
}

void
MessageToJson::writeScalar(
  const FieldIdentity & identity,
  const FieldCPtr & field,
  JsonWriter & writer) const
{
  writer.writeKey(fieldKey(identity));

  switch(field->getType())
  {
  case ValueType::INT8:
    writer.writeInt64(field->toInt8());
    break;
  case ValueType::INT16:
    writer.writeInt64(field->toInt16());
    break;
  case ValueType::EXPONENT:
  case ValueType::INT32:
    writer.writeInt64(field->toInt32());
    break;
  case ValueType::MANTISSA:
  case ValueType::INT64:
    writer.writeInt64(field->toInt64());
    break;
  case ValueType::UINT8:
    writer.writeUInt32(field->toUInt8());
    break;
  case ValueType::UINT16:
    writer.writeUInt32(field->toUInt16());
    break;
  case ValueType::LENGTH:
  case ValueType::UINT32:
    writer.writeUInt32(field->toUInt32());
    break;
  case ValueType::UINT64:
    writer.writeUInt64AsString(field->toUInt64());
    break;
  case ValueType::DECIMAL:
    writer.writeString(formatDecimalExact(field->toDecimal()));
    break;
  case ValueType::ASCII:
    {
      const StringBuffer & value = field->toAscii();
      writer.writeString(
        reinterpret_cast<const char *>(value.data()),
        value.size());
      break;
    }
  case ValueType::UTF8:
    {
      const StringBuffer & value = field->toUtf8();
      writer.writeString(
        reinterpret_cast<const char *>(value.data()),
        value.size());
      break;
    }
  case ValueType::BYTEVECTOR:
    {
      const StringBuffer & value = field->toByteVector();
      if(options_.byteVectors == JsonOptions::ByteVectorEncoding::Hex)
      {
        writer.writeString(encodeHex(value.data(), value.size()));
      }
      else
      {
        writer.writeString(encodeBase64(value.data(), value.size()));
      }
      break;
    }
  default:
    writer.writeNull();
    break;
  }
}

std::string
MessageToJson::fieldKey(const FieldIdentity & identity) const
{
  if(options_.keyMode == JsonOptions::KeyMode::Id)
  {
    if(!identity.id().empty())
    {
      return identity.id();
    }
  }
  return identity.getLocalName();
}

std::string
MessageToJson::encodeBase64(const unsigned char * data, size_t length)
{
  std::string encoded;
  encoded.reserve(((length + 2) / 3) * 4);

  size_t i = 0;
  while(i + 2 < length)
  {
    const unsigned n =
      (static_cast<unsigned>(data[i]) << 16) |
      (static_cast<unsigned>(data[i + 1]) << 8) |
      static_cast<unsigned>(data[i + 2]);
    encoded.push_back(base64Alphabet[(n >> 18) & 0x3f]);
    encoded.push_back(base64Alphabet[(n >> 12) & 0x3f]);
    encoded.push_back(base64Alphabet[(n >> 6) & 0x3f]);
    encoded.push_back(base64Alphabet[n & 0x3f]);
    i += 3;
  }

  if(i < length)
  {
    unsigned n = static_cast<unsigned>(data[i]) << 16;
    encoded.push_back(base64Alphabet[(n >> 18) & 0x3f]);
    if(i + 1 < length)
    {
      n |= static_cast<unsigned>(data[i + 1]) << 8;
      encoded.push_back(base64Alphabet[(n >> 12) & 0x3f]);
      encoded.push_back(base64Alphabet[(n >> 6) & 0x3f]);
      encoded.push_back('=');
    }
    else
    {
      encoded.push_back(base64Alphabet[(n >> 12) & 0x3f]);
      encoded.push_back('=');
      encoded.push_back('=');
    }
  }

  return encoded;
}

std::string
MessageToJson::encodeHex(const unsigned char * data, size_t length)
{
  static const char hexDigits[] = "0123456789abcdef";
  std::string encoded;
  encoded.reserve(length * 2);
  for(size_t i = 0; i < length; ++i)
  {
    encoded.push_back(hexDigits[(data[i] >> 4) & 0x0f]);
    encoded.push_back(hexDigits[data[i] & 0x0f]);
  }
  return encoded;
}
