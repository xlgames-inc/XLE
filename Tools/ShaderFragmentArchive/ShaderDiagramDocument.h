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

namespace ShaderDiagram
{
    public ref class Document
    {
    public:
        property Dictionary<String^, Object^>^       PreviewMaterialState
        {
            Dictionary<String^, Object^>^ get() { return _previewMaterialState; }
        }

        property Float3 NegativeLightDirection
        {
            Float3 get() { return *_negativeLightDirection; }
            void set(Float3 newValue) { *_negativeLightDirection = newValue; }
        }

        Document();
        ~Document();
    private:
        Dictionary<String^, Object^>^       _previewMaterialState;
        Float3* _negativeLightDirection;
    };

}
