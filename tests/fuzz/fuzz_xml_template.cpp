// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// libFuzzer entry for XMLTemplateParser over untrusted template documents.
#include <Common/QuickFASTPch.h>

#include <Codecs/XMLTemplateParser.h>

#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  if(size > 65536)
  {
    return 0;
  }

  try
  {
    const std::string xml(reinterpret_cast<const char *>(data), size);
    std::stringstream stream(xml);
    QuickFAST::Codecs::XMLTemplateParser parser;
    (void)parser.parse(stream);
  }
  catch(const std::exception &)
  {
  }
  catch(...)
  {
  }
  return 0;
}
