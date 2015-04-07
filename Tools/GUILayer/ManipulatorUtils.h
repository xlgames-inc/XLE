// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

namespace ToolsRig { class IManipulator; }

namespace GUILayer
{
    public ref class IManipulatorSet abstract
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) = 0;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() = 0;
        virtual ~IManipulatorSet();
    };
}

