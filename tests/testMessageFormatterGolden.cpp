// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
//
// MessageFormatter is what operators read when diagnosing a bad decode. A
// silent change in how scalars, decimals or byte vectors render is a silent
// change in every support dump.
#include <Common/QuickFASTPch.h>

#include <gtest/gtest.h>
#include <Messages/Message.h>
#include <Messages/MessageFormatter.h>
#include <Messages/FieldIdentity.h>
#include <Messages/FieldInt32.h>
#include <Messages/FieldUInt32.h>
#include <Messages/FieldAscii.h>
#include <Messages/FieldUtf8.h>
#include <Messages/FieldDecimal.h>
#include <Messages/FieldByteVector.h>

using namespace QuickFAST;

namespace
{
  // FieldSet stores a reference to the identity, so these must outlive the
  // message — the same constraint applications already live with.
  const Messages::FieldIdentity qty("qty", "", "32");
  const Messages::FieldIdentity px("px", "", "44");
  const Messages::FieldIdentity sym("sym", "", "55");
  const Messages::FieldIdentity note("note", "", "58");
  const Messages::FieldIdentity raw("raw", "", "96");
  const Messages::FieldIdentity seq("seq", "", "34");
}

/// @brief A message of ordinary scalars formats to a stable golden string.
TEST(QuickFAST, testMessageFormatterGoldenScalars)
{
  Messages::Message message(8);
  message.addField(qty, Messages::FieldInt32::create(-7));
  message.addField(px, Messages::FieldDecimal::create(Decimal(12345, -2)));
  message.addField(sym, Messages::FieldAscii::create("ABC"));
  message.addField(note, Messages::FieldUtf8::create("hi\xc3\xa9"));
  message.addField(raw, Messages::FieldByteVector::create(std::string("\x01\x02", 2)));
  message.addField(seq, Messages::FieldUInt32::create(9));

  std::ostringstream out;
  Messages::MessageFormatter formatter(out);
  formatter.formatMessage(message);

  const std::string text = out.str();
  EXPECT_NE(std::string::npos, text.find("qty[32]=-7"));
  EXPECT_NE(std::string::npos, text.find("px[44]=123.45"));
  EXPECT_NE(std::string::npos, text.find("sym[55]=ABC"));
  EXPECT_NE(std::string::npos, text.find("note[58]=hi"));
  EXPECT_NE(std::string::npos, text.find("raw[96]="));
  EXPECT_NE(std::string::npos, text.find("seq[34]=9"));
}
