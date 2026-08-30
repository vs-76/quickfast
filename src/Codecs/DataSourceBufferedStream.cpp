// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Common/QuickFASTPch.h>
#include "DataSourceBufferedStream.h"
using namespace ::QuickFAST;
using namespace ::QuickFAST::Codecs;

DataSourceBufferedStream::DataSourceBufferedStream(std::istream & stream)
: first_(true)
, end_(0)
{
  stream.seekg(0,std::ios::end);
  end_ =  (size_t) stream.tellg();
  stream.seekg(0,std::ios::beg);
  buffer_.reset(new unsigned char[end_]);
  stream.read(reinterpret_cast<char *>(buffer_.get()), end_);
}

DataSourceBufferedStream::~DataSourceBufferedStream()
{
}

bool
DataSourceBufferedStream::getBuffer(const uchar *& buffer, size_t & size)
{
  if(!first_)
  {
    return false;
  }
  first_ = false;
  buffer = buffer_.get();
  size = end_;
  return true;
}
