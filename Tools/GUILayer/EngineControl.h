// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "DelayedDeleteQueue.h"
#include "CLIXAutoPtr.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include <memory>

namespace PlatformRig { class InputTranslator; }

using namespace System::Windows::Forms;
using namespace System::Drawing;

namespace GUILayer 
{
	class IWindowRig;
	class EngineControlPimpl;
	public ref class EngineControl abstract
	{
	public:
        void Render();
        void OnPaint(PaintEventArgs^);
        void OnResize(System::EventArgs^ e);

        IWindowRig& GetWindowRig();

        EngineControl(Control^ control);
		~EngineControl();
        !EngineControl();

    protected:
        void Evnt_KeyDown(Object^, KeyEventArgs^ e);
        void Evnt_KeyUp(Object^, KeyEventArgs^ e);
        void Evnt_KeyPress(Object^, KeyPressEventArgs^ e);
        void Evnt_MouseMove(Object^, MouseEventArgs^ e);
        void Evnt_MouseDown(Object^, MouseEventArgs^ e);
        void Evnt_MouseUp(Object^, MouseEventArgs^ e);
        void Evnt_MouseWheel(Object^, MouseEventArgs^ e);
        void Evnt_DoubleClick(Object^, MouseEventArgs^ e);
        void Evnt_FocusChange(Object ^sender, System::EventArgs ^e);

        virtual void Render(RenderCore::IThreadContext&, IWindowRig&) = 0;

    private:
        clix::auto_ptr<EngineControlPimpl> _pimpl;
        Control^ _control;
    };

    class EngineControlPimpl
    {
    public:
        std::unique_ptr<IWindowRig> _windowRig;
        std::unique_ptr<PlatformRig::InputTranslator> _inputTranslator;

        ~EngineControlPimpl();
    };
}


