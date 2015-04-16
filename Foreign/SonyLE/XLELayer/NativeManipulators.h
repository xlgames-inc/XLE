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

namespace XLELayer
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

    private:
        bool SendInputEvent(
            Drawing::Size viewportSize,
			Sce::Atf::Rendering::Camera^ camera, 
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        void SetupModifierKeys(RenderOverlays::DebuggingDisplay::InputSnapshot& evnt);

        ActiveManipulatorContext^ _manipContext;
        bool _pendingBeginDrag;
    };

    public ref class PropertyBridge : Sce::Atf::Applications::IPropertyEditingContext
    {
    public:
        property IEnumerable<Object^>^ Items { virtual IEnumerable<Object^>^ get(); }
        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
            { virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get(); }

        PropertyBridge(GUILayer::IPropertySource^ source);
        ~PropertyBridge();
    private:
        GUILayer::IPropertySource^ _source;
    };
}
