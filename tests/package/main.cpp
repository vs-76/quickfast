// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.

// Touches one type from each installed module so a header that never made it
// into the install tree shows up as a compile error, and so the static archive
// has to resolve symbols from Common, Codecs, Messages and Communication.
// <Application/QuickFAST.h> comes first: every QuickFAST header requires it.
#include <Application/QuickFAST.h>

#include <Communication/HostResolver.h>
#include <Messages/FieldUInt64.h>
#include <Messages/MessageToJson.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
  QuickFAST::Decimal price(3142, -2);
  price += QuickFAST::Decimal(8, -2);
  std::string shown;
  price.toString(shown);
  if(shown != "31.5")
  {
    std::cerr << "decimal round trip: expected 31.5, got " << shown << std::endl;
    return EXIT_FAILURE;
  }

  const QuickFAST::Messages::FieldCPtr field =
    QuickFAST::Messages::FieldUInt64::create(42);
  if(field->toUInt64() != 42)
  {
    std::cerr << "uint64 field round trip failed" << std::endl;
    return EXIT_FAILURE;
  }

  QuickFAST::Codecs::TemplateRegistryPtr registry;
  if(registry)
  {
    std::cerr << "default TemplateRegistryPtr should be empty" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << QuickFAST::QuickFAST_Product
            << "consumer smoke test passed (decimal=" << shown << ")"
            << std::endl;
  return EXIT_SUCCESS;
}
