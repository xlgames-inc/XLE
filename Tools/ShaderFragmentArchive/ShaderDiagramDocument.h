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
using namespace System::Runtime::Serialization;

namespace ShaderPatcherLayer
{
    [DataContract] public ref class NodeGraphContext
    {
    public:
        property GUILayer::RawMaterial^ DefaultsMaterial;
		[DataMember] Dictionary<String^, String^>^ Variables;

		NodeGraphContext() { Variables = gcnew Dictionary<String^, String^>(); }
    };
}

