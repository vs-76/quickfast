// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MESSAGETOJSON_H
#define MESSAGETOJSON_H
#include <Common/QuickFAST_Export.h>
#include <Messages/JsonOptions.h>
#include <Messages/Message_fwd.h>
#include <Messages/FieldSet_fwd.h>
#include <Messages/FieldIdentity_fwd.h>
#include <Messages/Field_fwd.h>
#include <iosfwd>

namespace QuickFAST{
  namespace Messages{
    /// @brief Convert a decoded FAST Message / FieldSet to JSON text.
    ///
    /// Structure mapping:
    ///   FieldSet / Message / Group → JSON object
    ///   Sequence                   → JSON array of objects
    ///   scalars                    → JSON number or string (see JsonOptions)
    ///
    /// Absent / undefined fields are omitted.
    ///
    /// @par Example
    /// Write one decoded message per line (NDJSON), keyed by field id and with
    /// byte vectors in hex:
    /// @code
    /// QuickFAST::Messages::JsonOptions options;
    /// options.keyMode = QuickFAST::Messages::JsonOptions::KeyMode::Id;
    /// options.byteVectors =
    ///   QuickFAST::Messages::JsonOptions::ByteVectorEncoding::Hex;
    ///
    /// QuickFAST::Messages::MessageToJson formatter(options);
    /// formatter.formatMessage(message, std::cout);
    /// std::cout << std::endl;
    /// @endcode
    ///
    /// @see QuickFAST::Examples::JsonMessageConsumer for a MessageConsumer that
    ///      does exactly this for every decoded message.
    class QuickFAST_Export MessageToJson
    {
    public:
      /// @brief Construct a formatter.
      /// @param options controls key naming, byte vector encoding, and typeRef output
      explicit MessageToJson(const JsonOptions & options = JsonOptions());
      ~MessageToJson();

      /// @brief Format an entire message as a single JSON object.
      void formatMessage(const Message & message, std::ostream & out) const;

      /// @brief Format a FieldSet (message body, group, or sequence entry).
      void formatFieldSet(const FieldSet & fields, std::ostream & out) const;

    private:
      MessageToJson(const MessageToJson &) = delete;
      MessageToJson & operator=(const MessageToJson &) = delete;

      void writeFieldSet(const FieldSet & fields, class JsonWriter & writer) const;
      void writeField(
        const FieldIdentity & identity,
        const FieldCPtr & field,
        class JsonWriter & writer) const;
      void writeSequence(
        const FieldIdentity & identity,
        const FieldCPtr & field,
        class JsonWriter & writer) const;
      void writeGroup(
        const FieldIdentity & identity,
        const FieldCPtr & field,
        class JsonWriter & writer) const;
      void writeScalar(
        const FieldIdentity & identity,
        const FieldCPtr & field,
        class JsonWriter & writer) const;
      std::string fieldKey(const FieldIdentity & identity) const;
      static std::string encodeBase64(const unsigned char * data, size_t length);
      static std::string encodeHex(const unsigned char * data, size_t length);

    private:
      JsonOptions options_;
    };
  }
}
#endif /* MESSAGETOJSON_H */
