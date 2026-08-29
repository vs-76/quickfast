// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include "PCapReader.h"
#ifdef _WIN32
#include <Winsock2.h>
#else
#include <netinet/in.h>
#endif

#ifdef QUICKFAST_HAVE_LIBPCAP
#include <pcap.h>
#endif // QUICKFAST_HAVE_LIBPCAP

using namespace QuickFAST;
using namespace Communication;

namespace
{
#pragma pack(push)
#pragma pack(1)
  /*
   * Copyright (c) 1999 - 2005 NetGroup, Politecnico di Torino (Italy)
   * Copyright (c) 2005 - 2006 CACE Technologies, Davis (California)
   * All rights reserved.
   *
   * Redistribution and use in source and binary forms, with or without
   * modification, are permitted provided that the following conditions
   * are met:
   *
   * 1. Redistributions of source code must retain the above copyright
   * notice, this list of conditions and the following disclaimer.
   * 2. Redistributions in binary form must reproduce the above copyright
   * notice, this list of conditions and the following disclaimer in the
   * documentation and/or other materials provided with the distribution.
   * 3. Neither the name of the Politecnico di Torino, CACE Technologies
   * nor the names of its contributors may be used to endorse or promote
   * products derived from this software without specific prior written
   * permission.
   *
   * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

  struct ethernetIIHeader{
    uchar dst_mac[6];
    uchar src_mac[6];
    uchar ether_type[2];
  };

  struct linuxCookedCaptureHeader{
    uint16 packetType;
    uint16 linkAddressType;
    uchar filler; // cooked Capture Headers do not get properly byte swapped (????)
    uchar linkAddressLength;
    uchar linkAddress[8]; // link address
    uint16 protocol;  /* s/b 0x8 for IP */
  };

  /* UDP header*/
  struct udp_header{
      uint16 sport;             // Source port
      uint16 dport;             // Destination port
      uint16 len;               // Datagram length
      uint16 crc;               // Checksum
  };

#pragma pack(pop)

  /// Link types this reader knows how to strip. libpcap defines these, but the
  /// values are wire constants and are named here so the OFF build still
  /// compiles.
  static const int linkTypeEthernet = 1;
  static const int linkTypeLinuxCooked = 113;

  /// An IPv4 header is at least five 4 byte words.
  static const size_t minimumIpHeaderLength = 20;

  /// @brief Copy a packed header out of the captured frame.
  ///
  /// The link-layer scan advances one byte at a time, so casting a packed
  /// struct onto frame + pos is both an unaligned load and a strict-aliasing
  /// violation. memcpy is neither, and compiles to the same load once inlined.
  ///
  /// @returns false if the header does not lie wholly within [pos, limit).
  template<typename HeaderType>
  bool copyHeader(
    const unsigned char * frame,
    size_t limit,
    size_t pos,
    HeaderType & header)
  {
    if(pos > limit || limit - pos < sizeof(HeaderType))
    {
      return false;
    }
    std::memcpy(&header, frame + pos, sizeof(HeaderType));
    return true;
  }
}

PCapReader::PCapReader()
: handle_(0)
, ok_(false)
, atEnd_(false)
, linktype_(0)
, verbose_(false)
{
}

PCapReader::~PCapReader()
{
  close();
}

void
PCapReader::close()
{
#ifdef QUICKFAST_HAVE_LIBPCAP
  if(handle_ != 0)
  {
    pcap_close(handle_);
  }
#endif // QUICKFAST_HAVE_LIBPCAP
  handle_ = 0;
}

bool
PCapReader::open(const char * filename)
{
  close();
  filename_ = (filename != 0) ? filename : "";
  errorMessage_.clear();
  atEnd_ = false;
  ok_ = false;

#ifdef QUICKFAST_HAVE_LIBPCAP
  char errbuf[PCAP_ERRBUF_SIZE] = {0};
  handle_ = pcap_open_offline(filename_.c_str(), errbuf);
  if(handle_ == 0)
  {
    // libpcap names the format it could not make sense of, which is the whole
    // point of delegating: "missing magic" was wrong for every file that had
    // a valid magic this reader simply did not know.
    errorMessage_ = errbuf;
    std::cerr << "Cannot read capture file " << filename_ << ": " << errorMessage_ << std::endl;
    return ok_;
  }

  linktype_ = pcap_datalink(handle_);
  if(verbose_)
  {
    const char * name = pcap_datalink_val_to_name(linktype_);
    std::cout << "PCapReader: opened " << filename_ << " link type "
              << (name != 0 ? name : "unknown") << " (" << linktype_ << ')' << std::endl;
  }
  ok_ = true;
#else
  errorMessage_ =
    "Capture file support was not compiled in; rebuild with -DQUICKFAST_USE_LIBPCAP=ON.";
  std::cerr << "Cannot read capture file " << filename_ << ": " << errorMessage_ << std::endl;
#endif // QUICKFAST_HAVE_LIBPCAP
  return ok_;
}

bool
PCapReader::rewind()
{
  // libpcap savefiles are forward-only streams, so starting over means
  // reopening the file.
  if(filename_.empty())
  {
    return false;
  }
  return open(filename_.c_str());
}

bool
PCapReader::good()const
{
  return ok_;
}

bool
PCapReader::atEnd()const
{
  return atEnd_;
}

const std::string &
PCapReader::errorMessage()const
{
  return errorMessage_;
}

void
PCapReader::setVerbose(bool verbose)
{
  verbose_ = verbose;
}

bool
PCapReader::read(const unsigned char *& buffer, size_t & size)
{
#ifdef QUICKFAST_HAVE_LIBPCAP
  if(!ok_)
  {
    return false;
  }

  size_t skipped = 0;
  size_t malformed = 0;
  bool found = false;

  while(!found)
  {
    pcap_pkthdr * header = 0;
    const unsigned char * frame = 0;
    const int status = pcap_next_ex(handle_, &header, &frame);
    if(status == 1)
    {
      // caplen is what was actually stored; len is what was on the wire. A
      // shorter caplen means the tail of the packet is simply not in the file.
      const size_t frameLength = header->caplen;
      if(header->caplen != header->len)
      {
        skipped += 1;
        if(verbose_)
        {
          std::cout << "PCapReader: truncated packet, captured " << header->caplen
                    << " of " << header->len << " bytes." << std::endl;
        }
        continue;
      }
      if(dissect(frame, frameLength, buffer, size))
      {
        found = true;
      }
      else
      {
        malformed += 1;
      }
    }
    else if(status == PCAP_ERROR_BREAK)
    {
      // Clean end of the savefile.
      atEnd_ = true;
      ok_ = false;
      break;
    }
    else
    {
      // status 0 means a live-capture timeout, which a savefile cannot
      // produce, so anything left is a read error.
      const char * text = pcap_geterr(handle_);
      errorMessage_ = (text != 0) ? text : "Unknown capture file read error.";
      std::cerr << "Error reading capture file " << filename_ << ": " << errorMessage_ << std::endl;
      ok_ = false;
      break;
    }
  }

  if(skipped != 0)
  {
    std::cerr << "Warning: ignoring " << skipped << " truncated packets." << std::endl;
  }
  if(malformed != 0)
  {
    std::cerr << "Warning: ignoring " << malformed
              << " packets with lengths inconsistent with the capture." << std::endl;
  }
  return found;
#else
  (void)buffer;
  (void)size;
  return false;
#endif // QUICKFAST_HAVE_LIBPCAP
}

#if defined(QUICKFAST_ENABLE_TEST_HOOKS)
bool
PCapReader::dissectFrameForTest(
  int linktype,
  const unsigned char * frame,
  size_t frameLength,
  const unsigned char *& buffer,
  size_t & size)
{
  const int saved = linktype_;
  linktype_ = linktype;
  const bool ok = dissect(frame, frameLength, buffer, size);
  linktype_ = saved;
  return ok;
}
#endif

bool
PCapReader::dissect(
  const unsigned char * frame,
  size_t frameLength,
  const unsigned char *& buffer,
  size_t & size)
{
  // Every length below arrives from the capture file, and all of them are
  // size_t: an unchecked subtraction wraps to ~2^64 and every later bound
  // derived from it becomes meaningless.
  size_t pos = 0;
  bool found = false;

  switch(linktype_)
  {
  case linkTypeEthernet:
    {
      if(verbose_)
      {
        std::cout << "PCapReader: Ethernet packet." << std::endl;
      }
      if(frameLength >= sizeof(ethernetIIHeader))
      {
        pos = sizeof(ethernetIIHeader);
        found = true;
      }
      break;
    }
  case linkTypeLinuxCooked:
    {
      if(verbose_)
      {
        std::cout << "PCapReader: Linux cooked socket packet." << std::endl;
      }
      if(frameLength >= sizeof(linuxCookedCaptureHeader))
      {
        pos = sizeof(linuxCookedCaptureHeader);
        found = true;
      }
      break;
    }
  default:
    {
      if(verbose_)
      {
        std::cout << "PCapReader: Other type of packet.  Checking for IP protocol flag." << std::endl;
      }
      // HACK! look for the IP protocol flag to mark the end of the link layer
      // header, bounded by the frame rather than by a corruptible counter.
      static const unsigned short IPProtocol = 0x0800;
      unsigned short protocol = 0;
      while(!found && copyHeader(frame, frameLength, pos, protocol))
      {
        if(ntohs(protocol) == IPProtocol)
        {
          found = true;
          pos += 2;
        }
        else
        {
          pos += 1;
        }
      }
      break;
    }
  }

  if(!found)
  {
    if(verbose_)
    {
      std::cout << "PCapReader: no IP header found in this frame." << std::endl;
    }
    return false;
  }

  size_t remaining = frameLength - pos;

  // The only IPv4 field needed here is the header length, in the low nibble of
  // the first byte, expressed in 4 byte units.
  if(remaining < minimumIpHeaderLength)
  {
    return false;
  }
  const size_t ipLen = (frame[pos] & 0xF) * 4;
  if(ipLen < minimumIpHeaderLength || ipLen > remaining)
  {
    if(verbose_)
    {
      std::cout << "PCapReader: implausible IP header length " << ipLen
                << " with " << remaining << " bytes in the frame." << std::endl;
    }
    return false;
  }
  pos += ipLen;
  remaining -= ipLen;

  udp_header udpHeader;
  if(!copyHeader(frame, frameLength, pos, udpHeader))
  {
    return false;
  }
  pos += sizeof(udp_header);
  remaining -= sizeof(udp_header);

  // udplen covers the udp header plus cargo and arrives in network byte order.
  // It is a wire value: below the header size it underflows the cargo size,
  // above what was captured it would point the caller past the end.
  const size_t udplen = ntohs(udpHeader.len);
  if(udplen < sizeof(udp_header) || udplen - sizeof(udp_header) > remaining)
  {
    if(verbose_)
    {
      std::cout << "PCapReader: implausible UDP length " << udplen
                << " with " << remaining << " bytes in the frame." << std::endl;
    }
    return false;
  }

  buffer = frame + pos;
  size = udplen - sizeof(udp_header);
  if(verbose_)
  {
    std::cout << "PCapReader: cargo at offset " << pos << ", " << size << " bytes." << std::endl;
  }
  return true;
}
