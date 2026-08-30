// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#ifdef _MSC_VER
# pragma once
#endif

#ifndef XMLTEMPLATEPARSER_FWD_H
#define XMLTEMPLATEPARSER_FWD_H
#ifndef QUICKFAST_HEADERS
#error Please include <Application/QuickFAST.h> preferably as a precompiled header file.
#endif //QUICKFAST_HEADERS

namespace QuickFAST
{
  namespace Util
  {
    class XMLTemplateParser;
    /// @brief A smart pointer to a XMLTemplateParser.
    typedef std::shared_ptr<XMLTemplateParser> XMLTemplateParserPtr;
    /// @brief A smart pointer to a const XMLTemplateParser.
    typedef std::shared_ptr<const XMLTemplateParser> XMLTemplateParserCPtr;
  }
}
#endif // XMLTEMPLATEPARSER_FWD_H
