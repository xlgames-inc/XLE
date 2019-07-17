// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

namespace ToolsRig { class IManipulator; }

using namespace System::Collections::Generic;

namespace GUILayer
{
    public ref class IPropertySource abstract
    {
    public:
        using PropertyDescriptorsType = System::ComponentModel::PropertyDescriptorCollection; 
            // IEnumerable<System::ComponentModel::PropertyDescriptor^>;

        property IEnumerable<System::Object^>^ Items { virtual IEnumerable<System::Object^>^ get() = 0; }
        property PropertyDescriptorsType^ PropertyDescriptors
            { virtual PropertyDescriptorsType^ get() = 0; }
        virtual ~IPropertySource();
    };

    public ref class IManipulatorSet abstract
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) = 0;
		virtual IEnumerable<System::String^>^ GetManipulatorNames() = 0;
        
        IPropertySource^ GetProperties(System::String^ name);
        virtual ~IManipulatorSet();
    };

    public interface class IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result);
        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value);
    };
}

