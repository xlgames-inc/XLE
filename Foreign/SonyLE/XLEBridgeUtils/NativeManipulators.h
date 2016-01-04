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

    public interface class IViewContext
    {
        property Drawing::Size ViewportSize { Drawing::Size get(); }
        property Sce::Atf::Rendering::Camera^ Camera { Sce::Atf::Rendering::Camera^ get(); }
        property GUILayer::EditorSceneManager^ SceneManager { GUILayer::EditorSceneManager^ get(); }
        property GUILayer::TechniqueContextWrapper^ TechniqueContext { GUILayer::TechniqueContextWrapper^ get(); }
        property GUILayer::EngineDevice^ EngineDevice { GUILayer::EngineDevice^ get(); }
    };

    /// <summary>Provides a bridge between the SCE level editor types and native manipulators<summary>
    /// Accesses native IManipulator methods internally, but takes Sce level editor types as
    /// method parameters.
    public ref class NativeManipulatorLayer
    {
    public:
        static property GUILayer::EditorSceneManager^ SceneManager;

        bool MouseMove(IViewContext^ vc, Point scrPt);
        void Render();
        void OnBeginDrag();
        void OnDragging(IViewContext^ vc, Point scrPt);
        void OnEndDrag(IViewContext^ vc, Point scrPt);
        void OnMouseWheel(IViewContext^ vc, Point scrPt, int delta);

        NativeManipulatorLayer(ActiveManipulatorContext^ manipContext);
        ~NativeManipulatorLayer();

    private:
        bool SendInputEvent(IViewContext^ vc, const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        ActiveManipulatorContext^ _manipContext;
        bool _pendingBeginDrag;
    };
}
