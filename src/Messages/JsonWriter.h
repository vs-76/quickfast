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
    class QuickFAST_Export JsonWriter
    {
    public:
      explicit JsonWriter(std::ostream & out);
      ~JsonWriter();

      void beginObject();
      void endObject();
      void beginArray();
      void endArray();

      /// @brief Emit a member name; the next write or begin* is its value.
      void writeKey(const std::string & key);

      void writeNull();
      void writeBool(bool value);
      void writeInt64(int64 value);
      void writeUInt32(uint32 value);
      void writeUInt64AsString(uint64 value);
      void writeString(const std::string & value);
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
