// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EngineControl.h"

namespace GUILayer 
{
	public ref class TestControl : public EngineControl
	{
	public:
		TestControl(System::Windows::Forms::Control^ control);
		~TestControl();

    protected:
        virtual bool Render(RenderCore::IThreadContext& threadContext, IWindowRig& windowRig) override;
	};
}


