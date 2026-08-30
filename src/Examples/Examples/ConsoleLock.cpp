// Copyright (c) 2009, Object Computing, Inc.
// Copyright (c) 2026, QuickFAST contributors.
// All rights reserved.
// See the file license.txt for licensing information.
#include <Examples/ExamplesPch.h>
#include "ConsoleLock.h"
using namespace QuickFAST;
using namespace Examples;

std::mutex ConsoleLock::consoleMutex;
