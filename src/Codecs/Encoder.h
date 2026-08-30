// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef ENCODER_H
#define ENCODER_H
#include "Encoder_fwd.h"
#include <Common/QuickFAST_Export.h>
#include <Codecs/Context.h>
#include <Codecs/DataDestination_fwd.h>
#include <Codecs/PresenceMap_fwd.h>
#include <Codecs/Template.h>
#include <Codecs/SegmentBody_fwd.h>
#include <Messages/MessageAccessor.h>

#include <Common/Exceptions.h>

namespace QuickFAST{
  namespace Codecs{
    /// @brief Encode incoming FAST messages.
    ///
    /// Create an instance of the Encoder providing a registry of the templates
    /// to be used to encode the message, then call encodeMessage to encode
    /// each message from a DataDestination.
    ///
    /// Like the Decoder, an Encoder holds dictionary state across messages, and
    /// the receiving decoder must see the same reset points. Encoding throws on
    /// data a template cannot express (a missing mandatory field, for example);
    /// tell the destination to discard the partial message when that happens.
    ///
    /// @par Example
    /// Encode one message and collect the FAST bytes:
    /// @code
    /// const QuickFAST::Messages::FieldIdentity quantity("quantity");
    ///
    /// QuickFAST::Messages::Message message(registry->maxFieldCount());
    /// message.addField(quantity, QuickFAST::Messages::FieldUInt32::create(100));
    ///
    /// QuickFAST::Codecs::Encoder encoder(registry);
    /// QuickFAST::Codecs::DataDestination destination;
    /// try
    /// {
    ///   encoder.encodeMessage(destination, 1, message); // 1 == template id
    /// }
    /// catch(const std::exception &)
    /// {
    ///   destination.clear();   // do not transmit a half-encoded message
    ///   throw;
    /// }
    ///
    /// std::string fast;
    /// destination.toString(fast);
    /// @endcode
    ///
    /// @see QuickFAST::Codecs::DataDestination for higher performance
    ///      alternatives to toString(), such as scatter/gather writes.
    class QuickFAST_Export Encoder : public Context
    {
    public:
      /// @brief Construct with a TemplateRegistry containing all templates to be used.
      /// @param registry A registry containing all templates to be used to encode messages.
      Encoder(Codecs::TemplateRegistryPtr registry);

      /// @brief Encode messages until the accessor is satisfied.
      ///
      /// MessageAccessor::pickTemplate() will be called to select a template.
      /// Each time it returns true (and a valid template ID) another message will be encoded.
      ///
      /// @param[out] destination where to write the encoded messages.
      /// @param[in] accessor to the fields to be encoded.
      void encodeMessages(
        DataDestination & destination,
        Messages::MessageAccessor & accessor);


      /// @brief Encode the next message using the specified template.
      /// @param[out] destination where to write the encoded messages.
      /// @param[in] templateId identifies the template to use for encoding.
      /// @param[in] accessor to the fields to be encoded.
      void encodeMessage(
        DataDestination & destination,
        template_id_t templateId,
        const Messages::MessageAccessor & accessor);

      /// @brief Encode a group field.
      ///
      /// @param[in] destination to which FAST data goes.
      /// @param[in] group defines the group [a subset of a template]
      /// @param[in] accessor to the fields to be encoded
      void
      encodeGroup(
        DataDestination & destination,
        const Codecs::SegmentBodyPtr & group,
        const Messages::MessageAccessor & accessor);

      /// @brief Encode a segment into a destination.
      ///
      /// @param[in] source supplies the FAST encoded data.
      /// @param[in] templateId identifies the template to be used during encoding
      /// @param[in] accessor to the fields to be encoded
      void encodeSegment(
        DataDestination & source,
        template_id_t templateId,
        const Messages::MessageAccessor & accessor);

      /// @brief Encode the body of a segment into a destination.
      ///
      /// @param[in] destination receives the FAST encoded data.
      /// @param[in] presenceMap is used to determine which fields are present
      ///        in the input.
      /// @param[in] segment defines the expected fields [part of a template]
      /// @param[in] accessor to which the encoded fields will be added
      void encodeSegmentBody(
        DataDestination & destination,
        Codecs::PresenceMap & presenceMap,
        const Codecs::SegmentBodyCPtr & segment,
        const Messages::MessageAccessor & accessor);
    private:
    };
  }
}
#endif // ENCODER_H
