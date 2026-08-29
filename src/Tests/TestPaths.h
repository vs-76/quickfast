// Copyright (c) 2026
// Helper for unit tests that locate repo resources via QUICKFAST_ROOT.
#ifndef QUICKFAST_TESTPATHS_H
#define QUICKFAST_TESTPATHS_H

#include <cstdlib>
#include <string>
#include <stdexcept>

namespace QuickFAST {
namespace TestPaths {

inline std::string root()
{
  const char * value = std::getenv("QUICKFAST_ROOT");
  if(value == 0 || value[0] == '\0')
  {
    throw std::runtime_error(
      "QUICKFAST_ROOT is not set; point it at the QuickFAST source tree "
      "(ctest sets this automatically).");
  }
  return std::string(value);
}

inline std::string resource(const char * relativePath)
{
  return root() + relativePath;
}

} // namespace TestPaths
} // namespace QuickFAST

#endif // QUICKFAST_TESTPATHS_H
