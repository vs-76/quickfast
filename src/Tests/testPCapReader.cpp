// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <string>

#include <Application/DecoderConfiguration.h>
#include <Application/DecoderConnection.h>
#include <Codecs/GenericMessageBuilder.h>
#include <Codecs/SingleMessageConsumer.h>

#include "TestPaths.h"

using namespace QuickFAST;

namespace
{
  std::string capture(const char * name)
  {
    return TestPaths::resource((std::string("/src/Tests/resources/pcap/") + name).c_str());
  }

  std::string templateFile()
  {
    return TestPaths::resource("/src/Tests/resources/unittest_mandatory.xml");
  }

  void configureFromCapture(const char * captureName)
  {
    Application::DecoderConfiguration configuration;
    configuration.setTemplateFileName(templateFile());
    configuration.setReceiverType(Application::DecoderConfiguration::PCAPFILE_RECEIVER);
    configuration.setPcapFileName(capture(captureName));

    // Nothing is expected to decode: these tests are about the capture-file
    // layer, so the consumer only has to exist.
    Codecs::SingleMessageConsumer consumer;
    Codecs::GenericMessageBuilder builder(consumer);
    Application::DecoderConnection connection;
    connection.configure(builder, configuration);
  }
}

TEST(QuickFAST, testUnreadableCaptureIsFatalRatherThanASilentStall)
{
  // Receiver::start() returns false when the capture cannot be opened, but
  // DecoderConnection discarded that result: the caller then ran an event loop
  // over a receiver that had never been started and no buffers allocated, so
  // the process reported the problem and waited forever for data that could
  // never arrive.  Refusing to configure turns that hang into a clean exit.
  EXPECT_THROW(configureFromCapture("modern.pcapng"), UsageError);
  EXPECT_THROW(configureFromCapture("not-a-capture.bin"), UsageError);
  EXPECT_THROW(configureFromCapture("no-such-file.pcap"), UsageError);
}
