// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "ManipulatorOverlay.h"
#include "XLELayerUtils.h"
#include "../../Tools/ToolsRig/IManipulator.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../RenderCore/IDevice.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include "../../Tools/GUILayer/MarshalString.h"
#include "../../Tools/GUILayer/AutoToShared.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../Tools/ToolsRig/VisualisationUtils.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringUtils.h"
#include <memory>

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
    template<typename ParamType>
        static const ParamType* FindParameter(
            const char name[], std::pair<ParamType*, size_t> params, bool caseInsensitive)
    {
        for (unsigned c=0; c<params.second; ++c) {
            bool match = false;
            if (caseInsensitive) {
                match = !XlCompareStringI(params.first[c]._name, name);
            } else {
                match = !XlCompareString(params.first[c]._name, name);
            }
            if (match) 
                return &params.first[c];
        }
        return nullptr;
    }

    

    public ref class ManipulatorPropertiesContext : public IPropertyEditingContext
    {
    public:
        property IEnumerable<Object^>^ Items
        {
            virtual IEnumerable<Object^>^ get()
            {
                auto result = gcnew List<Object^>();
                result->Add(this);
                return result; 
            }
        }

        property IEnumerable<System::ComponentModel::PropertyDescriptor^>^ PropertyDescriptors
        {
            virtual IEnumerable<System::ComponentModel::PropertyDescriptor^>^ get()
            {
                    // We must convert each property in the manipulator 
                    // into a property descriptor that can be used to 
                    //  
                return nullptr;
            }
        }

        ManipulatorPropertiesContext(std::shared_ptr<ToolsRig::IManipulator> manipulator)
        {
            _manipulator.reset(new std::shared_ptr<ToolsRig::IManipulator>(std::move(manipulator)));
        }

    protected:
        GUILayer::AutoToShared<ToolsRig::IManipulator> _manipulator;

        ref class Helper : public ::System::Dynamic::DynamicObject
        {
        public:
            bool TryGetMember(System::Dynamic::GetMemberBinder^ binder, Object^% result) override
            {
                auto nativeName = clix::marshalString<clix::E_UTF8>(binder->Name);
                auto floatParam = FindParameter(nativeName.c_str(), (*_manipulator)->GetFloatParameters(), binder->IgnoreCase);
                if (floatParam) {
                    result = gcnew Single(*(float*)PtrAdd(_manipulator.get()->get(), floatParam->_valueOffset));
                    return true;
                }

                auto boolParam = FindParameter(nativeName.c_str(), (*_manipulator)->GetBoolParameters(), binder->IgnoreCase);
                if (boolParam) {
                    result = gcnew Boolean(*(bool*)PtrAdd(_manipulator.get()->get(), floatParam->_valueOffset));
                    return true;
                }

                result = nullptr;
                return false;
            }
            bool TrySetMember(System::Dynamic::SetMemberBinder^ binder, Object^ value) override
            {
                auto nativeName = clix::marshalString<clix::E_UTF8>(binder->Name);
                auto floatParam = FindParameter(nativeName.c_str(), (*_manipulator)->GetFloatParameters(), binder->IgnoreCase);
                if (floatParam) {
                    *(float*)PtrAdd(_manipulator.get()->get(), floatParam->_valueOffset) = (float)value;
                    return true;
                }
                auto boolParam = FindParameter(nativeName.c_str(), (*_manipulator)->GetBoolParameters(), binder->IgnoreCase);
                if (boolParam) {
                    *(bool*)PtrAdd(_manipulator.get()->get(), floatParam->_valueOffset) = (bool)value;
                    return true;
                }
                return false;
            }

            Helper(std::shared_ptr<ToolsRig::IManipulator> manipulator)
            {
                _manipulator.reset(new std::shared_ptr<ToolsRig::IManipulator>(std::move(manipulator)));
            }
        protected:
            GUILayer::AutoToShared<ToolsRig::IManipulator> _manipulator;
        };
    };

	static unsigned GetMouseButtonState()
	{
		return ((GetKeyState(4) < 0) << 2)
			| ((GetKeyState(2) < 0) << 1)
			| ((GetKeyState(1) < 0) << 0)
			;
	}

    [Export(LevelEditorCore::IManipulator::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class TerrainManipulator : public LevelEditorCore::IManipulator
    {
    public:
        static property GUILayer::EditorSceneManager^ SceneManager;

        TerrainManipulator() 
        {
            _activeManipulatorName = "Raise and Lower";
        }

        virtual bool Pick(LevelEditorCore::ViewControl^ vc, Point scrPt)
        {
			using namespace RenderOverlays::DebuggingDisplay;
			InputSnapshot evnt(0, 0, 0, Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));
				// "return true" has two effects --
				//		1. sets the cursor to a moving cursor
				//		2. turns mouse down into "drag-begin" event
			return SendInputEvent(vc->Camera, evnt);
        }

        virtual void Render(LevelEditorCore::ViewControl^ vc)
        {
				//	We can't get any context information from here!
				//	EditorSceneManager must be a singleton, otherwise
				//	there's no way to get it. Ideally the ViewControl
				//	could tell us something, but there's no way to attach
				//	more context information on the render call
			GUILayer::EditorSceneManager^ scene = SceneManager;
			if (!scene) return;

			auto manip = scene->GetManipulator(_activeManipulatorName);
            if (!manip) return;

            if (!ManipulatorOverlay::s_currentParsingContext) return;

            manip->Render(
                GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext().get(),
                *ManipulatorOverlay::s_currentParsingContext);
        }

        virtual void OnBeginDrag() {}
        virtual void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
			// we need to create a fake "mouse over" event and pass it through to
			// the currently selected manipulator. We might also need to set the state
			// for buttons and keys pressed down
			using namespace RenderOverlays::DebuggingDisplay;
			InputSnapshot evnt(
				GetMouseButtonState(), 0, 0,
				Coord2(scrPt.X, scrPt.Y), Coord2(0, 0));

			SendInputEvent(vc->Camera, evnt);
		}

        virtual void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt) 
		{
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

    private:
        String^ _activeManipulatorName;

		bool SendInputEvent(
			Sce::Atf::Rendering::Camera^ camera, 
			const RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
		{
			GUILayer::EditorSceneManager^ scene = SceneManager;
			if (!scene) return false;

			auto manip = scene->GetManipulator(_activeManipulatorName);
			if (!manip) return false;

			auto hitTestContext = GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
				GUILayer::EngineDevice::GetInstance(), nullptr,
				XLELayerUtils::AsCameraDesc(camera));
			auto hitTestScene = SceneManager->GetIntersectionScene();

			manip->OnInputEvent(evnt, hitTestContext->GetNative(), hitTestScene->GetNative());
			delete hitTestContext;
			delete hitTestScene;
			return true;
		}
    };

}


