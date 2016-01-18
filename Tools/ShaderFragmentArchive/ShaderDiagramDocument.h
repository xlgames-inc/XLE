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

        // Restrictions placed on the input variables
        [DataMember] property Dictionary<String^, String^>^ Variables { Dictionary<String^, String^>^ get() { if (!_variables) _variables = gcnew Dictionary<String^, String^>(); return _variables; } }

        // Configuration settings for the output file
        [DataMember] bool HasTechniqueConfig;
        [DataMember] property Dictionary<String^, String^>^ ShaderParameters { Dictionary<String^, String^>^ get() { if (!_shaderParameters) _shaderParameters = gcnew Dictionary<String^, String^>(); return _shaderParameters; } }

		NodeGraphContext() { HasTechniqueConfig = false; }

    private:
        Dictionary<String^, String^>^ _variables = nullptr;
        Dictionary<String^, String^>^ _shaderParameters = nullptr;
    };
}

