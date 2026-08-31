// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#ifdef _MSC_VER
# pragma once
#endif
#ifndef PCAPFILERECEIVER_H
#define PCAPFILERECEIVER_H
// All inline, do not export.
//#include <Common/QuickFAST_Export.h>
#include "PCapFileReceiver_fwd.h"
#include <Communication/SynchReceiver.h>
#include <Communication/PCapReader.h>

namespace QuickFAST
{
  namespace Communication
  {
    /// A Receiver that reads input from an istream.
    class PCapFileReceiver
      : public SynchReceiver
    {
    public:
      /// @brief Wrap a PCapFileReader into a Receiver
      ///
      /// @param filename names the capture file
      PCapFileReceiver(
        const std::string & filename
        )
        : SynchReceiver()
        , filename_(filename)
      {
      }

      ~PCapFileReceiver()
      {
      }

    private:

      // Implement Receiver method
      virtual bool initializeReceiver()
      {
        return reader_.open(filename_.c_str());
      }

      // Implement Receiver method
      bool fillBuffer(LinkedBuffer * buffer, std::unique_lock<std::mutex>& lock)
      {
        const unsigned char * pcapBuffer;
        size_t pcapSize;
        bool result = reader_.read(pcapBuffer, pcapSize);
        if(result)
        {
          if(pcapSize > buffer->capacity())
          {
            return false;
          }
          memcpy(buffer->get(), pcapBuffer, pcapSize);
          acceptFullBuffer(buffer, pcapSize, lock);
        }
        return result;
      }

      // Implement Receiver method
      virtual void resetService()
      {
        return;
      }
    private:
        std::string filename_;
      PCapReader reader_;
    };
  }
}
#endif // PCAPFILERECEIVER_H
