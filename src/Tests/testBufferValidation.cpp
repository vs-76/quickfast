// Copyright (c) 2026 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
// A buffer pool of zero cannot satisfy any read, and nothing noticed.
//
// The constraint is stated three times in DecoderConfiguration.h --
// "bufferCount_ * bufferSize_ must equal or exceed maximum message size" --
// and zero times anything is zero, so the documented precondition was violated
// and never checked. The result was no diagnostic, no exit and no progress,
// which is the worst possible answer to a configuration typo because it looks
// like the tool is working.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/XMLTemplateParser.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/SingleMessageConsumer.h>
#include <Codecs/MessagePerPacketAssembler.h>
#include <Messages/ValueMessageBuilder.h>
#include <Codecs/GenericMessageBuilder.h>

#include <Common/Exceptions.h>

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

  /// @brief A configuration pointed at a readable empty file.
  Application::DecoderConfiguration baseConfiguration(
    const std::string & fastFile)
  {
    Application::DecoderConfiguration configuration;
    configuration.setFastFileName(fastFile);
    configuration.setReceiverType(
      Application::DecoderConfiguration::BUFFERED_RAWFILE_RECEIVER);
    configuration.setAssemblerType(
      Application::DecoderConfiguration::MESSAGE_PER_PACKET_ASSEMBLER);
    return configuration;
  }

  /// @brief A readable input file with something in it.
  ///
  /// The contents do not matter: configure() only starts the receiver, and
  /// decoding happens later. An *empty* file would make start() fail for its
  /// own reasons and mask what is being tested here.
  std::string inputFile()
  {
    const std::string path = "testBufferValidation.fast";
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    out << "\xc0\x81\x81";
    out.close();
    return path;
  }

  void configureWith(Application::DecoderConfiguration & configuration)
  {
    std::stringstream templateStream{std::string(theTemplate)};
    Codecs::XMLTemplateParser parser;

    Application::DecoderConnection connection;
    connection.setTemplateRegistry(parser.parse(templateStream));

    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    connection.configure(builder, configuration);
  }
}

/// @brief A zero buffer size must be refused, not waited on forever.
TEST(QuickFAST, testZeroBufferSizeIsRejected)
{
  const std::string input = inputFile();
  Application::DecoderConfiguration configuration = baseConfiguration(input);
  configuration.setBufferSize(0);

  EXPECT_THROW(configureWith(configuration), UsageError);
  std::remove(input.c_str());
}

/// @brief A zero buffer count must be refused for the same reason.
TEST(QuickFAST, testZeroBufferCountIsRejected)
{
  const std::string input = inputFile();
  Application::DecoderConfiguration configuration = baseConfiguration(input);
  configuration.setBufferCount(0);

  EXPECT_THROW(configureWith(configuration), UsageError);
  std::remove(input.c_str());
}

/// @brief A workable pool is still accepted.
TEST(QuickFAST, testNonZeroBuffersAreAccepted)
{
  const std::string input = inputFile();
  Application::DecoderConfiguration configuration = baseConfiguration(input);
  configuration.setBufferSize(1500);
  configuration.setBufferCount(2);

  EXPECT_NO_THROW(configureWith(configuration));
  std::remove(input.c_str());
}
