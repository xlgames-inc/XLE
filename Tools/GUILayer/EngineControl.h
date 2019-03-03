// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"
#include "EngineDevice.h"
#include "../../RenderCore/IThreadContext_Forward.h"
#include <memory>

namespace PlatformRig { class InputTranslator; class InputContext; }

using namespace System::Drawing;

namespace GUILayer 
{
	class IWindowRig;
	class EngineControlPimpl;
    public ref class EngineControl abstract : public IOnEngineShutdown
	{
	public:
        bool Render();
        void OnPaint(System::Windows::Forms::Control^ ctrl, System::Windows::Forms::PaintEventArgs^);
        bool IsInputKey(System::Windows::Forms::Keys keyData);

        IWindowRig& GetWindowRig();

        EngineControl(System::Windows::Forms::Control^ control);
		~EngineControl();
        !EngineControl();
        virtual void OnEngineShutdown();

    protected:
        void Evnt_KeyDown(Object^, System::Windows::Forms::KeyEventArgs^ e);
        void Evnt_KeyUp(Object^, System::Windows::Forms::KeyEventArgs^ e);
        void Evnt_KeyPress(Object^, System::Windows::Forms::KeyPressEventArgs^ e);
        void Evnt_MouseMove(Object^, System::Windows::Forms::MouseEventArgs^ e);
        void Evnt_MouseDown(Object^, System::Windows::Forms::MouseEventArgs^ e);
        void Evnt_MouseUp(Object^, System::Windows::Forms::MouseEventArgs^ e);
        void Evnt_MouseWheel(Object^, System::Windows::Forms::MouseEventArgs^ e);
        void Evnt_DoubleClick(Object^, System::Windows::Forms::MouseEventArgs^ e);
        void Evnt_FocusChange(Object^, System::EventArgs ^e);
        void Evnt_Resize(Object^, System::EventArgs^ e);

        virtual bool Render(RenderCore::IThreadContext&, IWindowRig&) = 0;
		virtual void OnResize();

    private:
        clix::auto_ptr<EngineControlPimpl> _pimpl;

		PlatformRig::InputContext MakeInputContext(System::Windows::Forms::Control^ control);
    };

	class WindowRig;
    class EngineControlPimpl
    {
    public:
        std::unique_ptr<WindowRig> _windowRig;
        std::unique_ptr<PlatformRig::InputTranslator> _inputTranslator;

        ~EngineControlPimpl();
    };
}


