// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.

// <Application/QuickFAST.h> must come first: every QuickFAST header errors out
// without the macros it defines.
#include <Application/QuickFAST.h>

#include <Messages/FieldUInt64.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
  QuickFAST::Decimal price(3142, -2);
  std::string shown;
  price.toString(shown);
  if(shown != "31.42")
  {
    std::cerr << "decimal round trip: expected 31.42, got " << shown << std::endl;
    return EXIT_FAILURE;
  }

  const QuickFAST::Messages::FieldCPtr field =
    QuickFAST::Messages::FieldUInt64::create(7);
  if(field->toUInt64() != 7)
  {
    std::cerr << "uint64 field round trip failed" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "quickfast-ng package test passed" << std::endl;
  return EXIT_SUCCESS;
}
