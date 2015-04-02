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
#include "../../RenderCore/Techniques/Techniques.h"
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
    class TerrainManagerPimpl
    {
    public:
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;

        class RegisteredManipulator
        {
        public:
            std::string _name;
            std::shared_ptr<Tools::IManipulator> _manipulator;
            RegisteredManipulator(
                const std::string& name,
                std::shared_ptr<Tools::IManipulator> manipulator)
            : _name(name), _manipulator(manipulator)
            {}
            RegisteredManipulator() {}
        };
        std::vector<RegisteredManipulator> _manipulators;
    };

    private ref class TerrainManager
    {
    public:
        std::shared_ptr<Tools::IManipulator> GetManipulator(String^ name);
        IEnumerable<String^>^ GetManipulatorNames();

        std::shared_ptr<SceneEngine::TerrainManager> GetUnderlying() { return _pimpl->_terrainManager; }

        static TerrainManager^ GetActive(LevelEditorCore::ViewControl^) { return nullptr; }

        TerrainManager(String^ baseDir);
        ~TerrainManager();
    private:
        clix::auto_ptr<TerrainManagerPimpl> _pimpl;
    };

    public ref class TerrainGobAdapter : public Sce::Atf::Dom::DomNodeAdapter
    {
    protected:
        void OnNodeSet() override
        {
            __super::OnNodeSet();            
            auto node = DomNode;           
            node->AttributeChanged += gcnew EventHandler<Sce::Atf::Dom::AttributeEventArgs^>(this, &TerrainGobAdapter::node_AttributeChanged);
        }

        void node_AttributeChanged(Object^ sender, Sce::Atf::Dom::AttributeEventArgs^ e)
        {
            if (e->AttributeInfo->Name == "basedir") {
                TerrainManager = gcnew XLELayer::TerrainManager(
                    GetAttribute<System::String^>(e->AttributeInfo));
            }
        }

    internal:
        property TerrainManager^ TerrainManager;
    };

    std::shared_ptr<Tools::IManipulator> TerrainManager::GetManipulator(String^ name)
    {
        std::string nativeName = clix::marshalString<clix::E_UTF8>(name);
        for (auto i=_pimpl->_manipulators.cbegin(); i!=_pimpl->_manipulators.cend(); ++i)
            if (i->_name == nativeName)
                return i->_manipulator;
        return nullptr;
    }

    IEnumerable<String^>^ TerrainManager::GetManipulatorNames()
    {
        auto result = gcnew List<String^>();
        for (auto i=_pimpl->_manipulators.cbegin(); i!=_pimpl->_manipulators.cend(); ++i) {
            result->Add(clix::marshalString<clix::E_UTF8>(i->_name));
        }
        return result;
    }

    TerrainManager::TerrainManager(String^ baseDir)
    {
        _pimpl.reset(new TerrainManagerPimpl());

        SceneEngine::TerrainConfig cfg(clix::marshalString<clix::E_UTF8>(baseDir));
        _pimpl->_terrainManager = std::make_shared<SceneEngine::TerrainManager>(
            cfg,
            std::make_unique<RenderCore::Assets::TerrainFormat>(),
            GUILayer::EngineDevice::GetInstance()->GetNative().GetBufferUploads(),
            Int2(0,0), cfg._cellCount);

        // _pimpl->_manipulators.push_back(TerrainManagerPimpl::RegisteredManipulator("",...));
    }

    TerrainManager::~TerrainManager()
    {
        _pimpl.reset();
    }

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
            auto terrainManager = TerrainManager::GetActive(vc);
            if (!terrainManager) return false;
            
            auto ray = vc->GetWorldRay(scrPt);

            using namespace SceneEngine;
            IntersectionTestScene scene(terrainManager->GetUnderlying(), nullptr);
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
            auto terrainManager = TerrainManager::GetActive(vc);
            if (!terrainManager) return;

            auto manip = terrainManager->GetManipulator(_activeManipulatorName);
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


