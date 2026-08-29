// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// libFuzzer entry for PCapReader link/IP/UDP dissection (hand-written path).
#include <Common/QuickFASTPch.h>

#include <Communication/PCapReader.h>

#include <cstdint>
#include <cstring>

namespace {

// libpcap DLT values used by PCapReader::dissect.
constexpr int kDltEthernet = 1;
constexpr int kDltLinuxSll = 113;
constexpr int kDltRaw = 101; // "other" path: scan for EtherType 0x0800

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
  if(size < 1)
  {
    return 0;
  }

  // First byte selects the link-type branch; the rest is the frame.
  const int linkSelector = data[0] % 3;
  int linktype = kDltEthernet;
  if(linkSelector == 1)
  {
    linktype = kDltLinuxSll;
  }
  else if(linkSelector == 2)
  {
    linktype = kDltRaw;
  }

  const unsigned char * frame = data + 1;
  const size_t frameLength = size - 1;

  QuickFAST::Communication::PCapReader reader;
  const unsigned char * cargo = nullptr;
  size_t cargoSize = 0;
  (void)reader.dissectFrameForTest(linktype, frame, frameLength, cargo, cargoSize);

  // Touch the reported cargo so ASan sees any out-of-bounds pointer.
  if(cargo != nullptr && cargoSize > 0)
  {
    volatile unsigned char sink = 0;
    for(size_t i = 0; i < cargoSize; ++i)
    {
      sink = static_cast<unsigned char>(sink + cargo[i]);
    }
    (void)sink;
  }
  return 0;
}
