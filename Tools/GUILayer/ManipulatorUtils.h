// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

namespace ToolsRig { class IManipulator; }

using namespace System;
using namespace System::Collections::Generic;

namespace GUILayer
{
    public ref class IPropertySource abstract
    {
    public:
        property IEnumerable<Object^>^ Items { virtual IEnumerable<Object^>^ get() = 0; }
        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
            { virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get() = 0; }
        virtual ~IPropertySource();
    };

    public ref class IManipulatorSet abstract
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(String^ name) = 0;
		virtual IEnumerable<String^>^ GetManipulatorNames() = 0;
        
        IPropertySource^ GetProperties(String^ name);
        virtual ~IManipulatorSet();
    };
}

