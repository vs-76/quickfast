// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// libFuzzer entry for Decoder::decodeMessage over untrusted FAST bytes.
#include <Common/QuickFASTPch.h>

#include <Codecs/DataSourceBuffer.h>
#include <Codecs/Decoder.h>
#include <Codecs/TemplateRegistry.h>
#include <Codecs/XMLTemplateParser.h>

#include "DiscardingBuilder.h"

#include <cstdint>
#include <exception>
#include <mutex>
#include <sstream>
#include <string>

namespace {

std::once_flag g_registryOnce;
QuickFAST::Codecs::TemplateRegistryPtr g_registry;

void initRegistry()
{
  // Several field types so mutations reach more of the decoder than a
  // single-uint32 template would. Operators stay at nop for a dense wire form.
  static const char * const xml =
    "<templates>"
    "  <template name=\"fuzz\" id=\"1\">"
    "    <typeRef name=\"fuzz\"/>"
    "    <uInt32 name=\"u\"/>"
    "    <int32 name=\"i\" presence=\"optional\"/>"
    "    <string name=\"s\"/>"
    "    <decimal name=\"d\"/>"
    "    <sequence name=\"seq\">"
    "      <length name=\"len\"/>"
    "      <uInt32 name=\"item\"/>"
    "    </sequence>"
    "  </template>"
    "</templates>";

  std::stringstream stream(xml);
  QuickFAST::Codecs::XMLTemplateParser parser;
  g_registry = parser.parse(stream);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  // Cap pathological inputs; libFuzzer also has -max_len.
  if(size > 65536)
  {
    return 0;
  }

  try
  {
    std::call_once(g_registryOnce, initRegistry);
    if(!g_registry)
    {
      return 0;
    }

    QuickFAST::Codecs::DataSourceBuffer source(data, size);
    QuickFAST::Codecs::Decoder decoder(g_registry);
    QuickFAST::Fuzz::DiscardingBuilder builder;
    decoder.decodeMessage(source, builder);
  }
  catch(const std::exception &)
  {
    // Malformed FAST is expected; ASan/UBSan abort on real defects.
  }
  catch(...)
  {
  }
  return 0;
}
