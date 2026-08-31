// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// libFuzzer entry for FixedSizeHeaderAnalyzer over untrusted header bytes.
#include <Common/QuickFASTPch.h>

#include <Codecs/DataSourceBuffer.h>
#include <Codecs/FixedSizeHeaderAnalyzer.h>

#include <cstdint>
#include <exception>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  if(size < 2)
  {
    return 0;
  }

  // First two bytes configure the analyzer; the rest is the header stream.
  const size_t sizeBytes = 1u + (data[0] % 4);       // 1..4
  const bool bigEndian = (data[1] & 1) != 0;
  const size_t prefixBytes = (data[1] >> 1) & 0x7;   // 0..7
  const size_t suffixBytes = (data[1] >> 4) & 0x7;   // 0..7

  const unsigned char * header = data + 2;
  const size_t headerLength = size - 2;

  try
  {
    QuickFAST::Codecs::FixedSizeHeaderAnalyzer analyzer(
      sizeBytes, bigEndian, prefixBytes, suffixBytes);
    QuickFAST::Codecs::DataSourceBuffer source(header, headerLength);
    size_t blockSize = 0;
    bool skip = false;
    (void)analyzer.analyzeHeader(source, blockSize, skip);

    if(headerLength >= sizeBytes + prefixBytes + suffixBytes)
    {
      (void)analyzer.getSequenceNumber(header, headerLength);
    }
  }
  catch(const std::exception &)
  {
  }
  catch(...)
  {
  }
  return 0;
}
