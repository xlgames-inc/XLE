// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Math/Vector.h"

using System::Collections::Generic::Dictionary;
using System::String;
using System::Object;

namespace ShaderPatcherLayer
{
    public ref class Document
    {
    public:
        property GUILayer::RawMaterial^ DefaultsMaterial;
		property Dictionary<String^, String^>^ Variables;

		Document() { Variables = gcnew Dictionary<String^, String^>(); }
    };
}

