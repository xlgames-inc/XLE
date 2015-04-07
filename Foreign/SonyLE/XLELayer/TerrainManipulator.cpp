// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "ManipulatorOverlay.h"
#include "ManipulatorPropertyContext.h"
#include "XLELayerUtils.h"
#include "../../Tools/ToolsRig/IManipulator.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderCore/IDevice.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../Tools/ToolsRig/VisualisationUtils.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::ComponentModel;
using namespace System::ComponentModel::Composition;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace Sce::Atf;

using namespace Sce::Atf::VectorMath;
using namespace Sce::Atf::Adaptation;
using namespace Sce::Atf::Applications;
using namespace Sce::Atf::Controls::PropertyEditing;

namespace SceneEngine { class TerrainManager; }

extern "C" __declspec(dllimport) short __stdcall GetKeyState(int nVirtKey);

namespace XLELayer
{
	// static unsigned GetMouseButtonState()
	// {
	// 	return ((GetKeyState(4) < 0) << 2)
	// 		| ((GetKeyState(2) < 0) << 1)
	// 		| ((GetKeyState(1) < 0) << 0)
	// 		;
	// }

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

    GUILayer::IManipulatorSet^ ActiveManipulatorContext::ManipulatorSet::get() { return _manipulatorSet; }
    void ActiveManipulatorContext::ManipulatorSet::set(GUILayer::IManipulatorSet^ value)
    {
        if (value != _manipulatorSet) {
            _manipulatorSet = value;
            OnManipulatorSetChange(this, nullptr);
        }
    }

    String^ ActiveManipulatorContext::ActiveManipulator::get() { return _activeManipulator; }
    void ActiveManipulatorContext::ActiveManipulator::set(String^ value)
    {
        if (value != _activeManipulator) {
            _activeManipulator = value;
            OnActiveManipulatorChange(this, nullptr);
        }
    }

    ActiveManipulatorContext::ActiveManipulatorContext()
    {
        _manipulatorSet = nullptr;
        _activeManipulator = "";
    }

    public interface class ITerrainControls
    {
    public:
        property ActiveManipulatorContext^ ActiveContext { virtual void set(ActiveManipulatorContext^); }
    };

    private ref class DomChangeInspector
    {
    public:
        delegate void OnChangedDelegate(System::Object^);
        event OnChangedDelegate^ OnActiveContextChanged;
        event OnChangedDelegate^ OnDOMObjectChanged;

        DomChangeInspector(IContextRegistry^ contextRegistry) 
            : m_contextRegistry(contextRegistry)
        {
            m_observableContext = nullptr;
            contextRegistry->ActiveContextChanged += 
                gcnew EventHandler(this, &DomChangeInspector::ContextRegistry_ActiveContextChanged);
        }

    private:
        void ContextRegistry_ActiveContextChanged(System::Object^ sender, EventArgs^ e)
        {
            using namespace LevelEditorCore;
            IGameContext^ game = m_contextRegistry->GetActiveContext<IGameContext^>();
            auto observableContext = Sce::Atf::Adaptation::Adapters::As<IObservableContext^>(game);
            if (m_observableContext == observableContext) return;
            if (m_observableContext != nullptr) {
                m_observableContext->ItemInserted -= gcnew EventHandler<ItemInsertedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemInserted);
                m_observableContext->ItemRemoved -= gcnew EventHandler<ItemRemovedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemRemoved);
                m_observableContext->ItemChanged -= gcnew EventHandler<Sce::Atf::ItemChangedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemChanged);
                m_observableContext->Reloaded -= gcnew EventHandler(this, &DomChangeInspector::m_observableContext_Reloaded);
            }
            m_observableContext = observableContext;

            if (m_observableContext != nullptr) {
                m_observableContext->ItemInserted += gcnew EventHandler<ItemInsertedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemInserted);
                m_observableContext->ItemRemoved += gcnew EventHandler<ItemRemovedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemRemoved);
                m_observableContext->ItemChanged += gcnew EventHandler<Sce::Atf::ItemChangedEventArgs<System::Object^>^>(this, &DomChangeInspector::m_observableContext_ItemChanged);
                m_observableContext->Reloaded += gcnew EventHandler(this, &DomChangeInspector::m_observableContext_Reloaded);
            }
            OnActiveContextChanged(sender);
        }

        void m_observableContext_ItemChanged(System::Object^ sender, Sce::Atf::ItemChangedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_ItemRemoved(System::Object^ sender, ItemRemovedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_ItemInserted(System::Object^ sender, ItemInsertedEventArgs<System::Object^>^ e) { OnDOMObjectChanged(e->Item); }
        void m_observableContext_Reloaded(System::Object^ sender, EventArgs^ e) { OnDOMObjectChanged(nullptr); }
        
        IObservableContext^ m_observableContext;
        IContextRegistry^ m_contextRegistry;
    };

    ToolsRig::IManipulator* ActiveManipulatorContext::GetNativeManipulator()
    {
        auto set = ManipulatorSet;
        auto active = ActiveManipulator;
		if (!set || !active) return nullptr;
		return set->GetManipulator(active).get();
    }

    [Export(LevelEditorCore::IManipulator::typeid)]
    [Export(IInitializable::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class TerrainManipulator : public LevelEditorCore::IManipulator, public IInitializable
    {
    public:
        static property GUILayer::EditorSceneManager^ SceneManager;

        virtual bool Pick(LevelEditorCore::ViewControl^ vc, Point scrPt)
        {
			using namespace RenderOverlays::DebuggingDisplay;
			InputSnapshot evnt(0, 0, 0, Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
				// "return true" has two effects --
				//		1. sets the cursor to a moving cursor
				//		2. turns mouse down into "drag-begin" event
			return SendInputEvent(vc->ClientSize, vc->Camera, evnt);
        }
        virtual void Render(LevelEditorCore::ViewControl^ vc)
        {
				//	We can't get any context information from here!
				//	EditorSceneManager must be a singleton, otherwise
				//	there's no way to get it. Ideally the ViewControl
				//	could tell us something, but there's no way to attach
				//	more context information on the render call
            if (!ManipulatorOverlay::s_currentParsingContext) return;
            auto underlying = _manipContext->GetNativeManipulator();
            if (!underlying) return;

            underlying->Render(
                GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext().get(),
                *ManipulatorOverlay::s_currentParsingContext);
        }

        virtual void OnBeginDrag() { _pendingBeginDrag = true; }
        virtual void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
			    //  We need to create a fake "mouse over" event and pass it through to
			    //  the currently selected manipulator. We might also need to set the state
			    //  for buttons and keys pressed down.
                //  For the first "OnDragging" operation after a "OnBeginDrag", we should
                //  emulate a mouse down event.
			using namespace RenderOverlays::DebuggingDisplay;
            auto btnState = 1<<0;
			InputSnapshot evnt(
				btnState, _pendingBeginDrag ? btnState : 0, 0,
				Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
            SetupModifierKeys(evnt);

			SendInputEvent(vc->ClientSize, vc->Camera, evnt);
            _pendingBeginDrag = false;
		}
        virtual void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
                // Emulate a "mouse up" operation 
            using namespace RenderOverlays::DebuggingDisplay;
            auto btnState = 1<<0;
			InputSnapshot evnt(
				0, btnState, 0,
				Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
            SetupModifierKeys(evnt);
            SendInputEvent(vc->ClientSize, vc->Camera, evnt);

			// we need to create operations and turn them into a transaction:
			// string transName = string.Format("Apply {0} brush", brush.Name);
			// 
			// GameContext context = m_designView.Context.As<GameContext>();
			// context.DoTransaction(
			// 	delegate
			// {
			// 	foreach(var op in m_tmpOps)
			// 		context.TransactionOperations.Add(op);
			// }, transName);
			// m_tmpOps.Clear();
		}

        property LevelEditorCore::ManipulatorInfo^ ManipulatorInfo
        {
            virtual LevelEditorCore::ManipulatorInfo^ get() 
            {
                return gcnew LevelEditorCore::ManipulatorInfo(
                    Sce::Atf::Localizer::Localize("Terrain", String::Empty),
                    Sce::Atf::Localizer::Localize("Activate Terrain editing", String::Empty),
                    LevelEditorCore::Resources::TerrainManipImage,
                    Keys::None);
            }
        }

        TerrainManipulator() 
        {
            _pendingBeginDrag = false;
        }

        virtual void Initialize()
        {
            _domChangeInspector = gcnew DomChangeInspector(m_contextRegistry);
            _domChangeInspector->OnActiveContextChanged += gcnew DomChangeInspector::OnChangedDelegate(this, &TerrainManipulator::UpdateManipulatorContext);
            _domChangeInspector->OnDOMObjectChanged += gcnew DomChangeInspector::OnChangedDelegate(this, &TerrainManipulator::OnDOMChange);

            _manipContext = gcnew ActiveManipulatorContext();
            _controls->ActiveContext = _manipContext;
        }
    private:
		bool SendInputEvent(
            Drawing::Size viewportSize,
			Sce::Atf::Rendering::Camera^ camera, 
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
		{
			auto underlying = _manipContext->GetNativeManipulator();
            if (!underlying) return false;

			auto hitTestContext = GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
				GUILayer::EngineDevice::GetInstance(), nullptr,
				XLELayerUtils::AsCameraDesc(camera),
                viewportSize.Width, viewportSize.Height);
			auto hitTestScene = SceneManager->GetIntersectionScene();

			underlying->OnInputEvent(evnt, hitTestContext->GetNative(), hitTestScene->GetNative());
			delete hitTestContext;
			delete hitTestScene;
			return true;
		}

        void SetupModifierKeys(RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
        {
            using namespace RenderOverlays::DebuggingDisplay;
            typedef InputSnapshot::ActiveButton ActiveButton;
            static auto shift = KeyId_Make("shift");
            static auto control = KeyId_Make("control");
            static auto alt = KeyId_Make("alt");

            if (GetKeyState(0x10) < 0) evnt._activeButtons.push_back(ActiveButton(shift, false, true));
            if (GetKeyState(0x11) < 0) evnt._activeButtons.push_back(ActiveButton(control, false, true));
            if (GetKeyState(0x12) < 0) evnt._activeButtons.push_back(ActiveButton(alt, false, true));
        }

        void OnDOMChange(System::Object^ object)
        {
            bool updateManipulators = false;
            if (!object && SceneManager) {
                updateManipulators = true;
            } else {
                auto node = Sce::Atf::Adaptation::Adapters::As<Sce::Atf::Dom::DomNodeAdapter^>(object);
                if (node && node->DomNode->Type->Name == "gap:terrainType") {
                    updateManipulators = true;
                }
            }

            if (updateManipulators) {
                UpdateManipulatorContext(object);
            }
        }

        void UpdateManipulatorContext(System::Object^ object)
        {
            if (SceneManager) {
                _manipContext->ManipulatorSet = SceneManager->CreateTerrainManipulators();
            } else {
                _manipContext->ManipulatorSet = nullptr;
            }
        }

        DomChangeInspector^ _domChangeInspector;
        ActiveManipulatorContext^ _manipContext;
        bool _pendingBeginDrag;

        [Import(AllowDefault = false)]
        ITerrainControls^ _controls;

        [Import(AllowDefault = false)]
        IContextRegistry^ m_contextRegistry;
    };

}


