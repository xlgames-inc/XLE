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

namespace GUILayer
{
    ref class CameraDescWrapper;
    ref class EditorSceneManager;
    ref class TechniqueContextWrapper;
    ref class EngineDevice;
    ref class SimpleRenderingContext;
    ref class IManipulatorSet;

    public ref class ActiveManipulatorContext
    {
    public:
        ToolsRig::IManipulator* GetNativeManipulator();

        property IManipulatorSet^ ManipulatorSet    { IManipulatorSet^ get(); void set(IManipulatorSet^); }
        property String^ ActiveManipulator          { String^ get(); void set(String^); }

        void RaisePropertyChange();

        event EventHandler^ OnActiveManipulatorChange;
        event EventHandler^ OnManipulatorSetChange;

        ActiveManipulatorContext();
        ~ActiveManipulatorContext();
    private:
        IManipulatorSet^ _manipulatorSet;
        String^ _activeManipulator;
    };

    public interface class IViewContext
    {
        property Drawing::Size ViewportSize { Drawing::Size get(); }
        property CameraDescWrapper^ Camera { CameraDescWrapper^ get(); }
        property EditorSceneManager^ SceneManager { EditorSceneManager^ get(); }
        property TechniqueContextWrapper^ TechniqueContext { TechniqueContextWrapper^ get(); }
        property EngineDevice^ EngineDevice { GUILayer::EngineDevice^ get(); }
    };

    /// <summary>Provides a bridge between the SCE level editor types and native manipulators<summary>
    /// Accesses native IManipulator methods internally, but takes Sce level editor types as
    /// method parameters.
    public ref class NativeManipulatorLayer
    {
    public:
        bool MouseMove(IViewContext^ vc, Point scrPt);
        void Render(SimpleRenderingContext^ context);

        bool OnHover(IViewContext^ vc, Point scrPt);
        bool OnBeginDrag(IViewContext^ vc, Point scrPt);
        bool OnDragging(IViewContext^ vc, Point scrPt);
        bool OnEndDrag(IViewContext^ vc, Point scrPt);
        void OnMouseWheel(IViewContext^ vc, Point scrPt, int delta);

        NativeManipulatorLayer(ActiveManipulatorContext^ manipContext);
        ~NativeManipulatorLayer();

    private:
        bool SendInputEvent(IViewContext^ vc, const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);
        ActiveManipulatorContext^ _manipContext;
    };
}
