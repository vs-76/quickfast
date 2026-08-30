// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef JSONWRITER_H
#define JSONWRITER_H
#include <Common/QuickFAST_Export.h>
#include <Common/Types.h>
#include <iosfwd>
#include <string>
#include <vector>

namespace QuickFAST{
  namespace Messages{
    /// @brief Minimal streaming JSON writer (no third-party dependency).
    ///
    /// Emits compact JSON. Callers must balance begin/end for objects and
    /// arrays. After writeKey(), the next write* / begin* call is the value.
    ///
    /// @par Example
    /// @code
    /// std::ostringstream out;
    /// QuickFAST::Messages::JsonWriter writer(out);
    /// writer.beginObject();
    ///   writer.writeKey("symbol");
    ///   writer.writeString("MSFT");
    ///   writer.writeKey("quantity");
    ///   writer.writeInt64(100);
    ///   writer.writeKey("legs");
    ///   writer.beginArray();
    ///     writer.beginObject();
    ///       writer.writeKey("price");
    ///       writer.writeString("31.42");   // exact: not an IEEE double
    ///     writer.endObject();
    ///   writer.endArray();
    /// writer.endObject();
    /// // out.str() == {"symbol":"MSFT","quantity":100,"legs":[{"price":"31.42"}]}
    /// @endcode
    class QuickFAST_Export JsonWriter
    {
    public:
      /// @brief Construct a writer that appends to a stream.
      /// @param out receives the JSON text; must outlive this writer
      explicit JsonWriter(std::ostream & out);
      ~JsonWriter();

      /// @brief Open a JSON object ('{').
      void beginObject();
      /// @brief Close the innermost object ('}').
      void endObject();
      /// @brief Open a JSON array ('[').
      void beginArray();
      /// @brief Close the innermost array (']').
      void endArray();

      /// @brief Emit a member name; the next write or begin* is its value.
      /// @param key the member name, escaped as needed
      void writeKey(const std::string & key);

      /// @brief Emit a JSON null.
      void writeNull();
      /// @brief Emit a JSON boolean.
      /// @param value the value to emit
      void writeBool(bool value);
      /// @brief Emit a signed integer as a JSON number.
      /// @param value the value to emit
      void writeInt64(int64 value);
      /// @brief Emit an unsigned 32 bit integer as a JSON number.
      /// @param value the value to emit
      void writeUInt32(uint32 value);
      /// @brief Emit a 64 bit unsigned value as a JSON string.
      ///
      /// Quoting keeps the value exact for consumers whose numbers are IEEE
      /// doubles (JavaScript, for example).
      /// @param value the value to emit
      void writeUInt64AsString(uint64 value);
      /// @brief Emit a JSON string.
      /// @param value the text to emit; escaped as needed
      void writeString(const std::string & value);
      /// @brief Emit a JSON string from a counted buffer.
      /// @param value start of the text; escaped as needed
      /// @param length number of bytes at @p value
      void writeString(const char * value, size_t length);

    private:
      JsonWriter(const JsonWriter &) = delete;
      JsonWriter & operator=(const JsonWriter &) = delete;

      void separate();
      void writeQuoted(const char * value, size_t length);

    private:
      std::ostream & out_;
      std::vector<bool> needComma_;
      bool expectValue_;
    };
  }
}
#endif /* JSONWRITER_H */
