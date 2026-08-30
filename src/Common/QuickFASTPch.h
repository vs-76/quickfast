// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
# pragma warning(disable:4251) // Disable VC warning about dll linkage required (for private members?)
# pragma warning(disable:4275) // disable warning about non dll-interface base class.
# pragma warning(disable:4996) // Disable VC warning that std library may be unsafe
# pragma warning(disable:4290) // C4290: C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
# pragma warning(disable:4396) // Disable 'boost::operator !=' : the inline specifier cannot be used when a friend declaration refers to a specialization of a function template
                               // boost::unordered_set triggers this.  I think it's a bug somewhere, but it doesn't
                               // cause any problems because the code never compares boost::unordered sets
#pragma warning(disable:4820)  // 'n' bytes padding added after data member
#pragma warning(disable:4127)  // Conditonal expression is constant (particularly in templates)
#pragma warning(disable:4100)  // Disable: unreferenced formal parameter (/W4 warning: common case for virtual methods)

#endif
#ifndef QUICKFASTPCH_H
#define QUICKFASTPCH_H
// If this symbol is not defined the user included a QuickFAST header without
// using one of the standard precompiled header files.
#define QUICKFAST_HEADERS

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN      // Exclude rarely-used stuff from Windows headers
# endif
# ifndef NOMINMAX
#   define NOMINMAX                 // Do not define min & max a macros: l'histoire anciene
# endif
# include <windows.h>
#endif // _WIN32

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <cassert>

#include <cstdint>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stack>
#include <stdexcept>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <limits>
#include <cstdio>
#include <cstring>

////////////////////////
// Doxygen documentation

/// @mainpage
/// QuickFAST is a native C++ implementation of the FIX Adapted for STreaming
/// (FAST) protocol, with an optional .NET wrapper.
///
/// The protocol specification is published by the FIX Trading Community:
/// <a href="https://www.fixtrading.org/standards/fast/">
/// fixtrading.org/standards/fast</a>.
///
/// Source for this fork lives at
/// <a href="https://github.com/vs-76/quickfast">github.com/vs-76/quickfast</a>.
/// See BUILD.md in the source tree for build instructions (CMake with either
/// Conan 2 or vcpkg).
///
/// <h3>FAST in one page</h3>
/// FAST is a standard for serializing an <b><i>application data type</i></b>
/// into a <b><i>message</i></b>.
/// The goal is to minimize the size of the encoded message, and so the
/// bandwidth needed to transmit it, without spending excessive CPU to do so.
///
/// This is achieved by defining a custom encoding/decoding strategy per message
/// type out of a set of basic codec instructions. That strategy is called a
/// <b><i>template</i></b>, and more than one is usually defined.
/// The correspondence between application data types and templates is
/// many-to-many: an encoder may choose whichever template encodes a given
/// message most effectively.
///
/// Much of the compression comes from operators (copy, delta, increment, tail)
/// that describe a field relative to the previous message. Encoder and decoder
/// therefore each carry <i>dictionary</i> state, and a stream must be decoded in
/// order by a single decoder. Datagram transports (UDP, multicast) normally
/// reset that state per packet; see QuickFAST::Codecs::Context::reset.
///
/// Templates are most commonly written as XML documents; see
/// QuickFAST::Codecs::XMLTemplateParser for the element-to-object mapping.
/// The template file is shared between counterparties out of band, and it is
/// vital that both sides use the same set of templates.
///
/// <h3>Receiving FAST data</h3>
///
/// QuickFAST::Application::DecoderConnection is the recommended entry point: one
/// instance supports one source of FAST data (a multicast group, a TCP session, a
/// capture file), configured by a
/// QuickFAST::Application::DecoderConfiguration.
///
/// @code
/// #include <Application/QuickFAST.h>
/// #include <Application/DecoderConnection.h>
/// #include <Codecs/GenericMessageBuilder.h>
/// #include <Examples/JsonMessageConsumer.h>
///
/// using namespace QuickFAST;
///
/// Application::DecoderConfiguration configuration;
/// configuration.setTemplateFileName("templates.xml");
/// configuration.setFastFileName("data.fast");
/// configuration.setReceiverType(
///   Application::DecoderConfiguration::RAWFILE_RECEIVER);
/// configuration.setAssemblerType(
///   Application::DecoderConfiguration::STREAMING_ASSEMBLER);
///
/// // The consumer receives each decoded message; the builder assembles one.
/// // Both must outlive the decoding run.
/// Examples::JsonMessageConsumer consumer(std::cout);
/// Codecs::GenericMessageBuilder builder(consumer);
///
/// Application::DecoderConnection connection;
/// connection.configure(builder, configuration);
/// connection.run();   // returns when the receiver stops
/// @endcode
///
/// To take the data somewhere other than standard out, implement
/// QuickFAST::Codecs::MessageConsumer (working with an assembled
/// QuickFAST::Messages::Message, as above) or, for the lowest overhead,
/// implement QuickFAST::Messages::ValueMessageBuilder and skip the intermediate
/// Message entirely.
///
/// Two bundled applications are worth reading before writing your own:<ul>
///  <li><b>TutorialApplication</b> (src/Examples/TutorialApplication) shows
///         QuickFAST hard-configured for one use case.</li>
///  <li><b>InterpretApplication</b> (src/Examples/InterpretApplication) drives
///         every option that does not require custom code from the command
///         line, and is the quickest way to inspect an unfamiliar feed.</li>
/// </ul>
///
/// <h4>Decoding without a connection</h4>
/// When the data is already in hand, drive the decoder directly:
/// QuickFAST::Codecs::Decoder for one message,
/// QuickFAST::Codecs::SynchronousDecoder to loop over a
/// QuickFAST::Codecs::DataSource, or QuickFAST::Codecs::MulticastDecoder for a
/// self-contained multicast reader. Each of those pages carries an example.
///
/// <h3>Sending FAST data</h3>
/// @code
/// #include <Application/QuickFAST.h>
/// #include <Codecs/XMLTemplateParser.h>
/// #include <Codecs/Encoder.h>
/// #include <Codecs/DataDestination.h>
/// #include <Messages/Message.h>
/// #include <Messages/FieldUInt32.h>
///
/// using namespace QuickFAST;
///
/// std::ifstream templateFile("templates.xml");
/// Codecs::XMLTemplateParser parser;
/// Codecs::TemplateRegistryPtr registry = parser.parse(templateFile);
///
/// Codecs::Encoder encoder(registry);
///
/// const Messages::FieldIdentity quantity("quantity");
/// Messages::Message message(registry->maxFieldCount());
/// message.addField(quantity, Messages::FieldUInt32::create(100));
///
/// Codecs::DataDestination destination;
/// encoder.encodeMessage(destination, 1, message);   // 1 == template id
/// @endcode
///
/// The encoder writes to the DataDestination and then calls
/// QuickFAST::Codecs::DataDestination::endMessage(), which is the destination's
/// cue to transmit. Catch exceptions from the encoder and tell the destination
/// to discard the partially encoded message.
///
/// <h3>Working with decoded data</h3>
/// A decoded QuickFAST::Messages::Message is a collection of
/// QuickFAST::Messages::Field values, each identified by a
/// QuickFAST::Messages::FieldIdentity. Two formatters are provided:<ul>
/// <li>QuickFAST::Messages::MessageToJson emits JSON (see
///     QuickFAST::Messages::JsonOptions for key naming and byte vector
///     encoding).</li>
/// <li>QuickFAST::Messages::MessageFormatter emits human readable text.</li>
/// </ul>
/// FAST decimals arrive as QuickFAST::Decimal, which keeps mantissa and exponent
/// separate so prices stay exact.
///
/// <h3>Logging</h3>
/// QuickFAST reports decoding and communication problems through
/// QuickFAST::Common::Logger. When built with QUICKFAST_USE_SPDLOG,
/// QuickFAST::Common::SpdlogLogger forwards those reports to an application
/// spdlog logger, and QuickFAST::Common::managed_file_sink_mt provides a file
/// sink with size and scheduled rotation, gzip, and retention.
///
/// <h3>Supporting applications</h3>
/// The Examples directory also holds tools that do not use the codec themselves
/// but help in testing QuickFAST-based applications:
/// @see QuickFAST::Examples::FileToTCP
/// @see QuickFAST::Examples::FileToMulticast
///
/// This page was generated from comments in src/Common/QuickFASTPch.h

/// @brief General utility/overhead classes used throughout the rest of the
/// system are in the ::QuickFAST namespace.
///
/// Source files for elements in this namespace are in the src/Common directory.
/// They include QuickFAST::Decimal, the exceptions QuickFAST throws
/// (QuickFAST::EncodingError, QuickFAST::OverflowError,
/// QuickFAST::TemplateDefinitionError, QuickFAST::UsageError and their
/// siblings), and the logging support in QuickFAST::Common.
///
/// <i>This page was generated from comments in src/Common/QuickFASTPch.h</i>
namespace QuickFAST{

  /// @brief A FAST encoder and decoder.
  ///
  /// This namespace focuses on templates, codecs, and the encoding/decoding process, not on
  /// the use of the application data.
  ///
  /// QuickFAST::Codecs::Encoder is the encoder. <br>
  /// QuickFAST::Codecs::Decoder is the decoder. <br>
  /// QuickFAST::Codecs::XMLTemplateParser parses the templates.
  ///
  ///  @see XMLTemplateParser for more detailed information about parsing XML into templates.
  ///
  /// Source files for elements in this namespace are in the src/Codecs directory.
  ///
  /// <i>This page was generated from comments in src/Common/QuickFASTPch.h</i>
  namespace Codecs{}
  /// @brief Classes for managing FAST application data: messages and fields.
  ///
  /// This namespace focuses on using the application data.  It has no knowledge
  /// of the encoding/decoding process.  In particular nothing in this namespace
  /// should know anything about the ::QuickFAST::Codecs namespace.
  ///
  /// Application data is sent to and from QuickFAST via QuickFAST::Messages::Message
  /// objects.  A Message is a collection of QuickFAST::Messages::Field objects.
  /// Each field within the message is identified by a QuickFAST::Messages::FieldIdentity.
  ///
  /// Source files for elements in this namespace are in src/Messages directory.
  ///
  /// <i>This page was generated from comments in src/Common/QuickFASTPch.h</i>
  namespace Messages{}

  /// @brief Classes involved in passing data to/from the Decoder/Encoder from/to the outside world.
  ///
  /// Classes in this namespace address the issues involved in:
  /// <ul>
  /// <li>reading data from communication sockets</li>
  /// <li>reading data from data files</li>
  /// <li>handling any framing or other non-FAST information in the incoming data.</li>
  /// <li>delivering the incoming data to a decoder</li>
  /// <li>accepting outgoing data from an encoder</li>
  /// <li>adding any framing or other non-FAST information to the outgoing data.</li>
  /// <li>writing outgoing data to communication sockets</li>
  /// <li>writing outgoing data to files</li>
  /// </ul>
  /// Source files for elements in this namespace are in src/Communication directory.
  namespace Communication{}

  /// @brief Wrapper classes that provide high-level support to Applications using QuickFAST for decoding.
  ///
  /// This namespace contains: <dl>
  ///
  /// <dt>DecoderConfiguration</dt>
  ///   <dd>Contains all the information necessary to configure a DecoderConnection.
  ///       It can also parse its own command line options. </dd>
  /// <dt>DecoderConnection</dt>
  ///  <dd>Supports a single source of FAST encoded input. Start here; see the
  ///      example on QuickFAST::Application::DecoderConnection.</dd>
  /// <dt>CommandArgParser</dt>
  ///  <dd>Dispatches command line arguments to CommandArgHandler implementations.</dd>
  /// </dl>
  ///
  /// Source files for elements in this namespace are in the src/Application directory.
  namespace Application{}

  /// @brief Sample applications and reusable sample components.
  ///
  /// Nothing in the QuickFAST libraries depends on this namespace; it exists to
  /// be read and copied. QuickFAST::Examples::JsonMessageConsumer and
  /// QuickFAST::Examples::MessageInterpreter are the message consumers the
  /// bundled applications use, and are usable as-is.
  ///
  /// Source files for elements in this namespace are in the src/Examples directory.
  namespace Examples{}

}
#endif // QUICKFASTPCH_H
