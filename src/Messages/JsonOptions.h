// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef JSONOPTIONS_H
#define JSONOPTIONS_H

namespace QuickFAST{
  namespace Messages{
    /// @brief Options controlling FAST message → JSON conversion.
    ///
    /// DECIMAL and UINT64 are always emitted as JSON strings so values remain
    /// exact for consumers that cannot represent them as IEEE doubles (for
    /// example JavaScript Number).
    struct JsonOptions
    {
      /// @brief How to choose the JSON object key for each field.
      enum class KeyMode
      {
        Name, ///< FieldIdentity::getLocalName()
        Id    ///< FieldIdentity::id(); falls back to local name if id is empty
      };

      /// @brief How to encode BYTEVECTOR field values.
      enum class ByteVectorEncoding
      {
        Base64,
        Hex
      };

      /// @brief Which FieldIdentity attribute names each JSON member.
      KeyMode keyMode = KeyMode::Name;
      /// When true, emit applicationType / applicationTypeNs at the object root
      /// when the FieldSet carries a typeRef.
      bool includeApplicationType = true;
      /// @brief Text encoding applied to BYTEVECTOR values.
      ByteVectorEncoding byteVectors = ByteVectorEncoding::Base64;
    };
  }
}
#endif /* JSONOPTIONS_H */
