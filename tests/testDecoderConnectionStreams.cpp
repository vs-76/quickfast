// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// DecoderConnection holds three stream pointers, each of which may own a heap
// fstream or alias a standard stream. Two carried an ownership flag and the
// third did not, so the destructor deleted std::cin.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Common/Exceptions.h>
#include "TempFile.h"
#include "TestPaths.h"

using namespace QuickFAST;

namespace
{
  const char * const theTemplate =
    "<templates>"
    "  <template name=\"t\" id=\"1\">"
    "    <typeRef name=\"t\"/>"
    "    <uInt32 name=\"value\"><nop/></uInt32>"
    "  </template>"
    "</templates>";

  /// @brief One FAST message: presence map, template id 1, value 1.
  const char * const theMessage = "\xc0\x81\x81";

  /// @brief Build and immediately destroy a configured connection.
  ///
  /// Destruction is the point: that is where an aliased standard stream used
  /// to be deleted.
  void configureAndDestroy(Application::DecoderConfiguration & configuration)
  {
    std::stringstream templateStream{std::string(theTemplate)};
    Codecs::XMLTemplateParser parser;

    Application::DecoderConnection connection;
    connection.setTemplateRegistry(parser.parse(templateStream));

    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    connection.configure(builder, configuration);
  }

  Application::DecoderConfiguration fileConfiguration(const std::string & file)
  {
    Application::DecoderConfiguration configuration;
    configuration.setFastFileName(file);
    configuration.setReceiverType(
      Application::DecoderConfiguration::BUFFERED_RAWFILE_RECEIVER);
    configuration.setAssemblerType(
      Application::DecoderConfiguration::MESSAGE_PER_PACKET_ASSEMBLER);
    return configuration;
  }
}

/// @brief Reading from standard input must not end in delete &std::cin.
///
/// -file cin is a documented invocation -- the special case exists precisely
/// to support piping -- and on any non-Windows platform it ended the process
/// with "free(): invalid size" and SIGABRT. The two ostream members already
/// carried ownership flags for exactly this reason; fastFile_ never got one.
/// On Windows, stdin is switched to binary mode so FAST bytes are not mangled.
TEST(QuickFAST, testFastFileCinIsNotDeleted)
{
  Application::DecoderConfiguration configuration = fileConfiguration("cin");
  try
  {
    configureAndDestroy(configuration);
  }
  catch(const UsageError &)
  {
    // Standard input is empty under the test harness, so the receiver
    // legitimately declines to start. configure() aliases fastFile_ to cin
    // before it gets that far, so the destructor -- which is what this test
    // is about -- still runs holding the pointer that used to be deleted.
  }
}

/// @brief An owned input file is still closed, and a real file still works.
TEST(QuickFAST, testFastFileFromDiskStillWorks)
{
  const TestPaths::TemporaryFile input(theMessage, ".fast");
  Application::DecoderConfiguration configuration = fileConfiguration(input.name());
  EXPECT_NO_THROW(configureAndDestroy(configuration));
}

/// @brief An aliased echo or verbose stream must survive destruction too.
TEST(QuickFAST, testStandardEchoAndVerboseStreamsAreNotDeleted)
{
  const TestPaths::TemporaryFile input(theMessage, ".fast");
  for(const char * stream : {"cout", "cerr"})
  {
    SCOPED_TRACE(stream);
    Application::DecoderConfiguration configuration = fileConfiguration(input.name());
    configuration.setEchoFileName(stream);
    configuration.setVerboseFileName(stream);
    EXPECT_NO_THROW(configureAndDestroy(configuration));
  }
}

/// @brief With no verbose file configured, nothing may form a reference from null.
///
/// The same defect as the echo stream below, at a site the audit did not name.
/// UBSan reports it on every run that parses a template without -vo, which is
/// every ordinary run.
TEST(QuickFAST, testNoVerboseFileDoesNotDereferenceNull)
{
  const TestPaths::TemporaryFile input(theMessage, ".fast");
  Application::DecoderConfiguration configuration = fileConfiguration(input.name());
  configuration.setTemplateFileName(
    QuickFAST::TestPaths::resource("/tests/resources/unittest_mandatory.xml"));
  ASSERT_TRUE(configuration.verboseFileName().empty());

  // No setTemplateRegistry, so configure() parses the template file itself,
  // which is the path that reaches setVerboseOutput.
  Application::DecoderConnection connection;
  Codecs::SingleMessageConsumer consumer;
  Codecs::GenericMessageBuilder builder(consumer);
  EXPECT_NO_THROW(connection.configure(builder, configuration));
}

/// @brief With no echo configured, nothing may form a reference from null.
///
/// echoFile_ stays null when -echo is absent and was passed to setEcho as
/// *echoFile_ for every assembler type. The null round-trips into
/// DataSource::getEcho()'s documented "0 means no echo", so it is harmless in
/// practice, but a compiler may assume a reference is non-null and
/// -fsanitize=null reports it.
TEST(QuickFAST, testNoEchoFileDoesNotDereferenceNull)
{
  const TestPaths::TemporaryFile input(theMessage, ".fast");
  Application::DecoderConfiguration configuration = fileConfiguration(input.name());
  ASSERT_TRUE(configuration.echoFileName().empty());
  EXPECT_NO_THROW(configureAndDestroy(configuration));
}
