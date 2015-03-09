// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "CLIXAutoPtr.h"

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections;
using namespace System::Windows::Forms;
using namespace System::Data;
using namespace System::Drawing;

namespace GUILayer 
{
    class EngineControlPimpl;
	public ref class EngineControl : public System::Windows::Forms::UserControl
	{
	public:
		EngineControl();

	protected:
		~EngineControl();

        virtual void OnPaint(PaintEventArgs^) override;
        virtual void OnPaintBackground(PaintEventArgs^) override;
        virtual void OnResize(EventArgs^ e) override;

	private:
		System::ComponentModel::Container ^components;

        clix::auto_ptr<EngineControlPimpl> _pimpl;

        void Evnt_KeyDown(Object^, KeyEventArgs^ e);
        void Evnt_KeyUp(Object^, KeyEventArgs^ e);
        void Evnt_KeyPress(Object^, KeyPressEventArgs^ e);
        void Evnt_MouseMove(Object^, MouseEventArgs^ e);
        void Evnt_MouseDown(Object^, MouseEventArgs^ e);
        void Evnt_MouseUp(Object^, MouseEventArgs^ e);
        void Evnt_DoubleClick(Object^, MouseEventArgs^ e);

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
		}
#pragma endregion
	};
}


