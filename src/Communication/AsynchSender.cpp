// Copyright (c) 2009, 2010, 2011, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
#include <Common/QuickFASTPch.h>
#include "AsynchSender.h"
#include <Communication/LinkedBuffer.h>
#include <Common/Exceptions.h>
using namespace QuickFAST;
using namespace Communication;

AsynchSender::AsynchSender(
  BufferRecycler & recycler,
  const char * name)
    : Sender(recycler)
    , name_(name)
    , ioService_()
    , keepAlive_(new WorkGuard(ioService_.get_executor()))
{
  //std::cout << "Asynch Sender {" << (void *)this << "} keeping ioService " << (void*) &ioService_ << " alive." << std::endl;
}

AsynchSender::AsynchSender(
  asio::io_context & ioService,
  BufferRecycler & recycler,
  const char * name)
    : Sender(recycler)
    , name_(name)
    , ioService_(ioService)
    , keepAlive_(new WorkGuard(ioService_.get_executor()))
{
//  std::cout << "Asynch Sender {" << (void *)this << "} keeping shared ioService " << (void*) &ioService_ << " alive." << std::endl;
}

AsynchSender::~AsynchSender()
{
//  std::cout << "Asynch Sender {" << (void *)this << "} exiting." << std::endl;
}

void
AsynchSender::stop()
{
//  std::cout << "Asynch Sender {" << (void *)this << "} releasing keep-alive on  ioService " << (void*) &ioService_ << std::endl;
  keepAlive_.reset();
}

void
AsynchSender::close()
{
  keepAlive_.reset();
}

void
AsynchSender::handleWrite(
  const asio::error_code& error,
  LinkedBuffer * buffer,
  size_t bytesWritten)
{
  size_t requested = buffer->used();
//        std::cout << "Wrote " << bytesWritten << " bytes from buffer " << (void *)(buffer->get()) << '[' << buffer->capacity() << ']' << std::endl;
  recycle(buffer);
  if(error)
  {
    std::stringstream msg;
    msg
      << "Error writing to " << name_
      << " " << error.message();
    throw CommunicationError(msg.str());
  }
  else if(bytesWritten != requested)
  {
    std::stringstream msg;
    msg
      << "Error writing to " << name_
      << ". Tried to write " << requested
      << " bytes.  Actually wrote " << bytesWritten << " bytes.";
    throw CommunicationError(msg.str());
  }
}
