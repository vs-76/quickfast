// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef MESSAGEFIELD_H
#define MESSAGEFIELD_H

#include "MessageField_fwd.h"
#include <Common/QuickFAST_Export.h>
#include <Messages/Field_fwd.h>
#include <Messages/FieldIdentity.h>
#include <Common/Profiler.h>
namespace QuickFAST{
  namespace Messages{
    /// @brief the representation of a field within a message.
    ///
    /// @warning A MessageField borrows its FieldIdentity; it does not own one.
    /// For a decoded message the identity belongs to a FieldInstruction inside
    /// the Codecs::TemplateRegistry, so the registry must outlive every Message
    /// decoded from it, and every Field, FieldSet and Sequence reached through
    /// one. Callers that build messages by hand are equally responsible for
    /// keeping their identities alive.
    ///
    /// Storing the identity by value would make this safe, but it costs about
    /// 15% of decode time; the identity was never designed for shared
    /// ownership even though it is immutable at encode and decode time.
    class QuickFAST_Export MessageField
    {
    public:
      /// @brief Construct from an identity and a typed value.
      MessageField(const FieldIdentity & identity, const FieldCPtr & field)
        : identity_(identity)
        , field_(field)
      {
      }

      /// @brief copy constructor
      /// @param rhs the source from which to copy
      MessageField(const MessageField & rhs)
        : identity_(rhs.identity_)
        , field_(rhs.field_)
      {
      }

      MessageField & operator=(const MessageField &) = delete;

    public:

      /// @brief get the name of the field
      /// @returns the fully qualified field name
      const std::string name()const
      {
        return identity_.name();
      }
      /// @brief get the identity of the field
      /// @returns the identifying information for this field
      const FieldIdentity & getIdentity()const
      {
        return identity_;
      }

      /// @brief get the value of the field
      /// @returns  a pointer to the Field
      const FieldCPtr & getField()const
      {
        return field_;
      }
    private:
      const FieldIdentity & identity_;
      FieldCPtr field_;
    };
  }
}
#endif // MESSAGEFIELD_H
