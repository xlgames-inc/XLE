// Copyright 2016 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include <exception>

namespace GUILayer
{
	System::Exception^ Marshal(const std::exception& e);
}