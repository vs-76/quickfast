// Copyright (c) 2011 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#ifdef _WIN32 // Asynchronous file writer only works on Win32 so disable this entire test on other platforms

#include <gtest/gtest.h>
#include "TestPaths.h"
#include <filesystem>

#include <Communication/BufferRecycler.h>
#include <Communication/AsynchFileSender.h>
#include <Communication/LinkedBuffer.h>
#include <Common/Exceptions.h>

#include <string.h>

using namespace QuickFAST;
namespace
{
  class AsynchronousFileCopier : public Communication::BufferRecycler
  {
    public:
#pragma warning(push) // we're on windows so don't worry about #ifdeffing around the pragmas
#pragma warning(disable:4355)//warning C4355: 'this' : used in base member initializer list

      AsynchronousFileCopier(
        const std::string & inputFilename,
        size_t bufferCount,
        size_t bufferSize,
        const std::string & outputFile)
        : sender_(*this, outputFile.c_str())
        , inputFileName_(inputFilename)
        , bufferCount_(bufferCount)
        , bufferSize_(bufferSize)
        , in_(0)
      {
      }
#pragma warning(pop)

      ~AsynchronousFileCopier()
      {
        if(in_ != 0)
        {
          std::fclose(in_);
          in_ = 0;
        }
      }

      void close()
      {
        if(in_ != 0)
        {
          std::fclose(in_);
          in_ = 0;
        }
      }

      void go(size_t threadCount = 2)
      {
        // binary because we're hoping for an exact copy
        in_ = fopen(inputFileName_.c_str(), "rb");
        if(in_ == 0)
        {
          throw CommunicationError(std::string("Can't open input file:") + inputFileName_);
        }
        sender_.open();
        for(size_t i = 0; !std::feof(in_) && i < bufferCount_; ++i)
        {
          Communication::LinkedBuffer * buffer = new Communication::LinkedBuffer(bufferSize_);
          startWrite(buffer);
        }
        sender_.runThreads(threadCount, true);
        sender_.close();
      }

      void startWrite(Communication::LinkedBuffer * buffer)
      {
        if(buffer == 0)
        {
          return;
        }
        // warning fails for files sizes larger than a long
        long offset = 0;
        {
          std::unique_lock<std::mutex> lock(fileMutex_);

          if(std::feof(in_))
          {
            delete buffer;
            sender_.stop();
            return;
          }
          offset = std::ftell(in_);
          size_t bytesRead = std::fread(buffer->get(), 1, buffer->capacity(), in_);
          if(bytesRead <= 0)
          {
            delete buffer;
            sender_.stop();
            return;
          }
          buffer->setUsed(bytesRead);
        } // unlock the mutex before sending
        sender_.sendAt(buffer, offset);
      }

      virtual void recycle(Communication::LinkedBuffer * buffer)
      {
        startWrite(buffer);
      }
    private:
      Communication::AsynchFileSender sender_;
      std::string inputFileName_;
      size_t bufferCount_;
      size_t bufferSize_;

      FILE * in_;
      // protects access to the file when we are potentially mutlithreading
      std::mutex fileMutex_;
    };
}


TEST(QuickFAST, TestAsynchFileWriter)
{
  std::string root = QuickFAST::TestPaths::root();
  std::string workingDirectory = root + "/tests/resources/";
  std::string inputFile = workingDirectory + "fileCopyTest.dat";
  std::string outputFile = workingDirectory + "fileCopyTest.out";
  std::filesystem::remove(outputFile);

  const size_t bufferCount = 5;
  const size_t bufferSize = 10; // force a lot of I/O

  AsynchronousFileCopier copier(
    inputFile,
    bufferCount,
    bufferSize,
    outputFile
    );
  copier.go();

  FILE * ifile = std::fopen(inputFile.c_str(), "rb");
  ASSERT_TRUE(ifile != 0);
  std::fseek(ifile, 0, SEEK_END);
  long ilen = ftell(ifile);
  std::fseek(ifile, 0, SEEK_SET);
  std::unique_ptr<char[]> ibuff(new char[ilen]);
  ASSERT_EQ((ilen), (std::fread(ibuff.get(), 1, ilen, ifile)));
  std::fclose(ifile);

  FILE * ofile = std::fopen(outputFile.c_str(), "rb");
  ASSERT_TRUE(ofile != 0);
  std::fseek(ofile, 0, SEEK_END);
  long olen = ftell(ofile);
  std::fseek(ofile, 0, SEEK_SET);
  std::unique_ptr<char[]> obuff(new char[olen]);
  ASSERT_EQ((olen), (std::fread(obuff.get(), 1, olen, ofile)));
  std::fclose(ofile);


  ASSERT_EQ((ilen), (olen));
  ASSERT_EQ((0), (std::memcmp(ibuff.get(), obuff.get(), ilen)));

  std::filesystem::remove(outputFile);
}

#endif // _WIN32
