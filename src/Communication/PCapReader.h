// Copyright (c) 2009, 2010, 2011 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif
#ifndef PCAPREADER_H
#define PCAPREADER_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

#include <Common/QuickFAST_Export.h>
#include <Common/Types.h>

#include <string>

struct pcap;

namespace QuickFAST
{
  namespace Communication
  {
    /// @brief Read a packet capture file containing UDP packets
    ///
    /// A simple file reader that handles only UDP (and multicast) packets.
    /// For more power, see tcpdump and/or the libpcap open source project.
    ///
    /// Capture files are read through libpcap, so classic pcap in either
    /// endianness and at either timestamp resolution, and pcapng, are all
    /// accepted without the caller having to know which it has. Building with
    /// -DQUICKFAST_USE_LIBPCAP=OFF leaves open() reporting that capture
    /// support was not compiled in.
    class QuickFAST_Export PCapReader
    {
    public:
      PCapReader();
      ~PCapReader();

      PCapReader(const PCapReader &) = delete;
      PCapReader & operator=(const PCapReader &) = delete;

      /// @brief open a packet capture file
      ///
      /// @param filename names the file
      /// @returns true if the open was successful
      bool open(const char * filename);

      /// @brief enable noisy operation for debugging purposes
      ///
      /// @param verbose true turns on the noise.
      void setVerbose(bool verbose);

      /// @brief Start over with the first record in the file.
      ///
      /// libpcap savefiles are read forward only, so this closes and reopens.
      /// @returns true if everything went ok.
      bool rewind();

      /// @brief Check the state of the file.
      ///
      /// @returns true if no errors and not at end of file.
      bool good()const;

      /// @brief Did the last read stop because the file ran out, rather than failed?
      ///
      /// @returns true after a clean end of file, false if reading failed.
      bool atEnd()const;

      /// @brief Describe why the reader is not good().
      ///
      /// @returns libpcap's diagnostic, or an empty string if nothing failed.
      const std::string & errorMessage()const;

      /// @brief Read the next record in the file.
      ///
      /// @warning The returned pointer belongs to libpcap and is valid only
      /// until the next call to read() or rewind(), or until this reader is
      /// destroyed. Copy the bytes before doing anything asynchronous with
      /// them.
      ///
      /// @param[out] buffer end up pointing to the user data in the packet (headers are bypassed)
      /// @param[out] size contains the number of bytes of user data in the packet (zero is possible and legal!)
      /// @returns true if the read was successful.  False usually means end of data
      bool read(const unsigned char *& buffer, size_t & size);

    private:
      /// Find the UDP cargo inside one captured link-layer frame.
      bool dissect(
        const unsigned char * frame,
        size_t frameLength,
        const unsigned char *& buffer,
        size_t & size);

      void close();

      pcap * handle_;
      std::string filename_;
      std::string errorMessage_;
      bool ok_;
      bool atEnd_;
      int linktype_;
      bool verbose_;
    };
  }
  }
#endif // PCAPREADER_H
