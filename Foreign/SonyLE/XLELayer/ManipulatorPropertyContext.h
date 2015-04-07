// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Tools/GUILayer/CLIXAutoPtr.h"

namespace ToolsRig { class IManipulator; }

namespace XLELayer
{
    public ref class ManipulatorPropertyContext : public Sce::Atf::Applications::IPropertyEditingContext
    {
    public:
        property System::Collections::Generic::IEnumerable<Object^>^ Items
        {
            virtual System::Collections::Generic::IEnumerable<Object^>^ get();
        }

        property System::Collections::Generic::IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
        {
            virtual System::Collections::Generic::IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get();
        }

            // note --  no protection on this pointer. Caller must ensure the
            //          native manipulator stays around for the life-time of 
            //          this object.
        ManipulatorPropertyContext(std::shared_ptr<ToolsRig::IManipulator> manipulator);
        ~ManipulatorPropertyContext();

        static ManipulatorPropertyContext^ Create(GUILayer::IManipulatorSet^ mani, System::String^ name);

    protected:
        ref class Helper;
        ref class DynamicPropertyDescriptor;
        clix::shared_ptr<ToolsRig::IManipulator> _manipulator;
        Helper^ _helper;
    };
}

