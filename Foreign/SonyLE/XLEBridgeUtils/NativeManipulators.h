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

    public ref class NativeManipulatorLayer
    {
    public:
        static property GUILayer::EditorSceneManager^ SceneManager;

        bool MouseMove(LevelEditorCore::ViewControl^ vc, Point scrPt);
        void Render(LevelEditorCore::ViewControl^ vc);
        void OnBeginDrag();
        void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt);
        void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt);

        NativeManipulatorLayer(ActiveManipulatorContext^ manipContext);
        ~NativeManipulatorLayer();

    private:
        bool SendInputEvent(
            Drawing::Size viewportSize,
			Sce::Atf::Rendering::Camera^ camera, 
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        ActiveManipulatorContext^ _manipContext;
        bool _pendingBeginDrag;
    };
}
