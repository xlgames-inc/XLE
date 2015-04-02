// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

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

#include "ManipulatorOverlay.h"
#include "../../PlatformRig/ManipulatorsUtil.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Assets/TerrainFormat.h"
// #include "../../RenderCore/Techniques/Techniques.h"
#include "../../Tools/GUILayer/CLIXAutoPtr.h"
#include "../../Tools/GUILayer/MarshalString.h"
#include "../../Tools/GUILayer/AutoToShared.h"
#include "../../Tools/GUILayer/NativeEngineDevice.h"
#include "../../Utility/PtrUtils.h"
#include "../../Utility/StringUtils.h"
#include <memory>

namespace SceneEngine { class TerrainManager; }

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

        ManipulatorPropertiesContext(std::shared_ptr<Tools::IManipulator> manipulator)
        {
            _manipulator.reset(new std::shared_ptr<Tools::IManipulator>(std::move(manipulator)));
        }

    protected:
        GUILayer::AutoToShared<Tools::IManipulator> _manipulator;

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

            Helper(std::shared_ptr<Tools::IManipulator> manipulator)
            {
                _manipulator.reset(new std::shared_ptr<Tools::IManipulator>(std::move(manipulator)));
            }
        protected:
            GUILayer::AutoToShared<Tools::IManipulator> _manipulator;
        };
    };

    static Float3 AsFloat3(Vec3F input) { return Float3(input.X, input.Y, input.Z); }

    [Export(LevelEditorCore::IManipulator::typeid)]
    [PartCreationPolicy(CreationPolicy::Shared)]
    public ref class TerrainManipulator : public LevelEditorCore::IManipulator
    {
    public:
        TerrainManipulator() 
        {
            _activeManipulatorName = "Raise and Lower";
        }

        virtual bool Pick(LevelEditorCore::ViewControl^ vc, Point scrPt)
        {
			auto scene = GUILayer::EditorSceneManager::GetInstance();
			if (!scene) return false;
            
            auto ray = vc->GetWorldRay(scrPt);

            using namespace SceneEngine;
            IntersectionTestContext testContext(
                GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext(),
                RenderCore::Techniques::CameraDesc(),
                std::make_shared<RenderCore::Techniques::TechniqueContext>());

            auto result = scene.FirstRayIntersection(
                testContext, 
                std::make_pair(AsFloat3(ray.Origin), AsFloat3(ray.Origin + vc->Camera->FarZ * ray.Direction)),
                IntersectionTestScene::Type::Terrain);

            return result._type != 0;
        }

        virtual void Render(LevelEditorCore::ViewControl^ vc)
        {
				//	We can't get any context information from here!
				//	EditorSceneManager must be a singleton, otherwise
				//	there's no way to get it. Ideally the ViewControl
				//	could tell us something, but there's no way to attach
				//	more context information on the render call
			auto scene = GUILayer::EditorSceneManager::GetInstance(vc);
			if (!scene) return;

			auto manip = scene->GetManipulator(_activeManipulatorName);
            if (!manip) return;

            if (!ManipulatorOverlay::s_currentParsingContext) return;

            manip->Render(
                GUILayer::EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext().get(),
                *ManipulatorOverlay::s_currentParsingContext);
        }
        
        virtual void OnBeginDrag() {}
        virtual void OnDragging(LevelEditorCore::ViewControl^ vc, Point scrPt) {}
        virtual void OnEndDrag(LevelEditorCore::ViewControl^ vc, Point scrPt) {}

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
    };

}


