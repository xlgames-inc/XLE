// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using namespace System;
using namespace System::Drawing;
using namespace System::Collections::Generic;

namespace RenderOverlays { namespace DebuggingDisplay
{
    class InputSnapshot;
}}

namespace ToolsRig { class IManipulator; }

namespace XLEBridgeUtils
{
    public ref class ActiveManipulatorContext
    {
    public:
        ToolsRig::IManipulator* GetNativeManipulator();

        property GUILayer::IManipulatorSet^ ManipulatorSet  { GUILayer::IManipulatorSet^ get(); void set(GUILayer::IManipulatorSet^); }
        property String^ ActiveManipulator                  { String^ get(); void set(String^); }

        event EventHandler^ OnActiveManipulatorChange;
        event EventHandler^ OnManipulatorSetChange;

        ActiveManipulatorContext();
        ~ActiveManipulatorContext();
    private:
        GUILayer::IManipulatorSet^ _manipulatorSet;
        String^ _activeManipulator;
    };

    /// <summary>Provides a bridge between the SCE level editor types and native manipulators<summary>
    /// Accesses native IManipulator methods internally, but takes Sce level editor types as
    /// method parameters.
    public ref class NativeManipulatorLayer
    {
    public:
        static property GUILayer::EditorSceneManager^ SceneManager;

        value class View
        {
        public:
            Drawing::Size _viewportSize;
			Sce::Atf::Rendering::Camera^ _camera;
        };

        bool MouseMove(View vc, Point scrPt);
        void Render();
        void OnBeginDrag();
        void OnDragging(View vc, Point scrPt);
        void OnEndDrag(View vc, Point scrPt);
        void OnMouseWheel(View vc, Point scrPt, int delta);

        NativeManipulatorLayer(ActiveManipulatorContext^ manipContext);
        ~NativeManipulatorLayer();

    private:
        bool SendInputEvent(View vc, const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        ActiveManipulatorContext^ _manipContext;
        bool _pendingBeginDrag;
    };
}
