// Copyright (c) 2009, Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#pragma once
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace QuickFAST {

/// @brief Minimal replacement for boost::lexical_cast.
/// Supports string/char* → arithmetic and arithmetic/pointer → string.
template<typename Target, typename Source>
inline Target lexical_cast(const Source & source)
{
  if constexpr (std::is_same_v<Target, std::decay_t<Source>>)
  {
    return source;
  }
  else if constexpr (std::is_same_v<Target, std::string>)
  {
    if constexpr (std::is_same_v<std::decay_t<Source>, std::string>)
    {
      return source;
    }
    else
    {
      std::ostringstream oss;
      oss << source;
      return oss.str();
    }
  }
  else
  {
    std::string text;
    if constexpr (std::is_same_v<std::decay_t<Source>, std::string>)
    {
      text = source;
    }
    else if constexpr (std::is_convertible_v<Source, const char *>)
    {
      const char * p = source;
      text = p ? p : "";
    }
    else
    {
      std::ostringstream oss;
      oss << source;
      text = oss.str();
    }

    std::istringstream iss(text);
    Target value{};
    if (!(iss >> value))
    {
      throw std::invalid_argument("lexical_cast failed");
    }
    // Be lenient like boost: allow trailing whitespace only.
    while (iss && !iss.eof())
    {
      const int ch = iss.peek();
      if (ch == EOF)
      {
        break;
      }
      if (!std::isspace(static_cast<unsigned char>(ch)))
      {
        throw std::invalid_argument("lexical_cast failed");
      }
      iss.get();
    }
    return value;
  }
}

} // namespace QuickFAST
