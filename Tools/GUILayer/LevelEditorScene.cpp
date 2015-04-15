// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "PlacementsGobInterface.h"
#include "TerrainGobInterface.h"
#include "MarshalString.h"
#include "GUILayerUtil.h"
#include "IOverlaySystem.h"
#include "EditorInterfaceUtils.h"
#include "UITypesBinding.h" // for VisCameraSettings
#include "ExportedNativeTypes.h"
#include "../ToolsRig/VisualisationUtils.h"     // for AsCameraDesc
#include "../ToolsRig/PlacementsManipulators.h"     // just needed for destructors referenced in PlacementGobInterface.h
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/TerrainMaterial.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/IThreadContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include <memory>

namespace GUILayer
{
    // Many level editors work around a structure of objects and attributes.
    // That is, the scene is composed of a hierachy of objects, and each object
    // as some type, and a set of attributes. In the case of the SonyWWS editor,
    // this structure is backed by a xml based DOM.
    // 
    // It's handy because it's simple and flexible. And we can store just about
    // anything like this.
    // 
    // When working with a level editor like this, we need a some kind of dynamic
    // "scene" object. This scene should react to basic commands from the editor:
    //
    //      * Create/destroy object (with given fixed type)
    //      * Set object attribute
    //      * Set object parent
    //
    // We also want to build in a "document" concept. Normally a document should
    // represent a single target file (eg a level file or some settings file).
    // Every object should belong to a single object, and all of the objects in
    // a single document should usually be part of one large hierarchy.
    //
    // For convenience when working with the SonyWWS editor, we want to be able to
    // pre-register common strings (like type names and object property names).
    // It might be ok just to use hash values for all of these cases. It depends on
    // whether we want to validate the names when they are first registered.

///////////////////////////////////////////////////////////////////////////////////////////////////

    EditorScene::EditorScene()
    {
        _placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
            SceneEngine::WorldPlacementsConfig(),
            std::shared_ptr<RenderCore::Assets::IModelFormat>(), Float2(0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
    }

	EditorScene::~EditorScene()
	{}

    class EditorSceneParser : public SceneEngine::ISceneParser
    {
    public:
        RenderCore::Techniques::CameraDesc GetCameraDesc() const
        {
            return AsCameraDesc(*_camera);
        }

        using DeviceContext = RenderCore::Metal::DeviceContext;
        using LightingParserContext = SceneEngine::LightingParserContext;
        using SceneParseSettings =  SceneEngine::SceneParseSettings;

        void ExecuteScene(  
            DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned techniqueIndex) const;
        void ExecuteShadowScene(    
            DeviceContext* context, 
            LightingParserContext& parserContext, 
            const SceneParseSettings& parseSettings,
            unsigned index, unsigned techniqueIndex) const;

        unsigned GetShadowProjectionCount() const;
        SceneEngine::ShadowProjectionDesc GetShadowProjectionDesc(
            unsigned index, 
            const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const;

        unsigned                        GetLightCount() const;
        const SceneEngine::LightDesc&   GetLightDesc(unsigned index) const;
        SceneEngine::GlobalLightingDesc GetGlobalLightingDesc() const;
        float                           GetTimeValue() const;

        EditorSceneParser(
            std::shared_ptr<EditorScene> editorScene,
            std::shared_ptr<ToolsRig::VisCameraSettings> camera);
        ~EditorSceneParser();
    protected:
        std::shared_ptr<EditorScene> _editorScene;
        std::shared_ptr<ToolsRig::VisCameraSettings> _camera;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    using namespace SceneEngine;

    void EditorSceneParser::ExecuteScene(  
        DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

			if (parseSettings._toggles & SceneParseSettings::Toggles::Terrain && _editorScene->_terrainGob && _editorScene->_terrainGob->_terrainManager) {
				TRY {
                    _editorScene->_terrainGob->_terrainManager->Render(context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
			}

            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                TRY {
                    _editorScene->_placementsManager->Render(
                        context, parserContext, techniqueIndex);
                }
                CATCH(const ::Assets::Exceptions::PendingResource& e) { parserContext.Process(e); }
                CATCH(const ::Assets::Exceptions::InvalidResource& e) { parserContext.Process(e); }
                CATCH_END
            }
        }
    }

    void EditorSceneParser::ExecuteShadowScene(    
        DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned index, unsigned techniqueIndex) const
    {
        ExecuteScene(context, parserContext, parseSettings, techniqueIndex);
    }

    unsigned EditorSceneParser::GetShadowProjectionCount() const { return 0; }

    ShadowProjectionDesc EditorSceneParser::GetShadowProjectionDesc(
        unsigned index, const RenderCore::Techniques::ProjectionDesc& mainSceneProjectionDesc) const 
    { return ShadowProjectionDesc(); }

    unsigned           EditorSceneParser::GetLightCount() const { return 1; }
    const LightDesc&   EditorSceneParser::GetLightDesc(unsigned index) const
    {
        static LightDesc light;
        light._type = LightDesc::Directional;
        light._lightColour = Float3(1.f, 1.f, 1.f);
        light._negativeLightDirection = Normalize(Float3(-.1f, 0.33f, 1.f));
        light._radius = 10000.f;
        light._shadowFrustumIndex = ~unsigned(0x0);
        return light;
    }

    GlobalLightingDesc EditorSceneParser::GetGlobalLightingDesc() const
    {
        GlobalLightingDesc result;
        result._ambientLight = .03f * Float3(1.f, 1.f, 1.f);
        result._skyTexture = "game/xleres/DefaultResources/sky/desertsky.jpg";
        result._doToneMap = true;
        return result;
    }

    float EditorSceneParser::GetTimeValue() const { return 0.f; }

    EditorSceneParser::EditorSceneParser(
        std::shared_ptr<EditorScene> editorScene,
        std::shared_ptr<ToolsRig::VisCameraSettings> camera)
        : _editorScene(std::move(editorScene)), _camera(std::move(camera))
    {}
    EditorSceneParser::~EditorSceneParser() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

    using namespace EditorDynamicInterface;

    DocumentId EditorSceneManager::CreateDocument(DocumentTypeId docType) 
        { return _dynInterface->CreateDocument(*_scene.get(), docType, ""); }
    bool EditorSceneManager::DeleteDocument(DocumentId doc, DocumentTypeId docType)
        { return _dynInterface->DeleteDocument(*_scene.get(), doc, docType); }

    ObjectId EditorSceneManager::AssignObjectId(DocumentId doc, ObjectTypeId type)
        { return _dynInterface->AssignObjectId(*_scene.get(), doc, type); }

    bool EditorSceneManager::CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return _dynInterface->CreateObject(*_scene.get(), doc, obj, objType, ""); }

    bool EditorSceneManager::DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return _dynInterface->DeleteObject(*_scene.get(), doc, obj, objType); }

    bool EditorSceneManager::SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const void* src, unsigned elementType, unsigned arrayCount)
        { return _dynInterface->SetProperty(*_scene.get(), doc, obj, objType, prop, src, elementType, arrayCount); }

    bool EditorSceneManager::GetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, size_t* destSize)
        { return _dynInterface->GetProperty(*_scene.get(), doc, obj, objType, prop, dest, destSize); }

    DocumentTypeId EditorSceneManager::GetDocumentTypeId(System::String^ name)                  { return _dynInterface->GetDocumentTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    ObjectTypeId EditorSceneManager::GetTypeId(System::String^ name)                            { return _dynInterface->GetTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    PropertyId EditorSceneManager::GetPropertyId(ObjectTypeId type, System::String^ name)       { return _dynInterface->GetPropertyId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }
    ChildListId EditorSceneManager::GetChildListId(ObjectTypeId type, System::String^ name)     { return _dynInterface->GetChildListId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }

    bool EditorSceneManager::SetObjectParent(DocumentId doc, 
            ObjectId childId, ObjectTypeId childTypeId, 
            ObjectId parentId, ObjectTypeId parentTypeId, int insertionPosition)
    {
        return _dynInterface->SetParent(
            *_scene.get(), doc, childId, childTypeId,
            parentId, parentTypeId, insertionPosition);
    }

    IManipulatorSet^ EditorSceneManager::CreateTerrainManipulators() 
    { 
        if (_scene->_terrainGob && _scene->_terrainGob->_terrainManager) {
            return gcnew TerrainManipulators(_scene->_terrainGob->_terrainManager);
        } else {
            return nullptr;
        }
    }

    IManipulatorSet^ EditorSceneManager::CreatePlacementManipulators(
        IPlacementManipulatorSettingsLayer^ context)
    {
        if (_scene->_placementsEditor) {
            return gcnew PlacementManipulators(context->GetNative(), _scene->_placementsEditor);
        } else {
            return nullptr;
        }
    }

	IntersectionTestSceneWrapper^ EditorSceneManager::GetIntersectionScene()
	{
		return gcnew IntersectionTestSceneWrapper(
            (_scene->_terrainGob) ? _scene->_terrainGob->_terrainManager : nullptr,
            _scene->_placementsEditor);
    }

    PlacementsEditorWrapper^ EditorSceneManager::GetPlacementsEditor()
	{
		return gcnew PlacementsEditorWrapper(_scene->_placementsEditor);
    }

    static ::Assets::rstring GetRString(const ParameterBox& paramBox, ParameterBox::ParameterNameHash name)
    {
        auto type = paramBox.GetParameterType(name);
        if (type._type == ImpliedTyping::TypeCat::Int8 || type._type == ImpliedTyping::TypeCat::UInt8) {
            ::Assets::rstring result;
            result.resize(std::max(1u, (unsigned)type._arrayCount));
            paramBox.GetParameter(name, AsPointer(result.begin()), type);
            return std::move(result);
        }

        if (type._type == ImpliedTyping::TypeCat::Int16 || type._type == ImpliedTyping::TypeCat::UInt16) {
            std::basic_string<wchar_t> wideResult;
            wideResult.resize(std::max(1u, (unsigned)type._arrayCount));
            paramBox.GetParameter(name, AsPointer(wideResult.begin()), type);

            ::Assets::rstring result;
            result.resize(std::max(1u, (unsigned)type._arrayCount));
            ucs2_2_utf8(
                (ucs2*)AsPointer(wideResult.begin()), wideResult.size(),
                (utf8*)AsPointer(result.begin()), result.size());
            return std::move(result);
        }

        return ::Assets::rstring();
    }

    static void UpdateTerrainBaseTexture(
        const EditorDynamicInterface::FlexObjectType& sys,
        const EditorDynamicInterface::FlexObjectType::Object& obj)
    {
        auto& divAsset = *Assets::GetDivergentAsset<SceneEngine::TerrainMaterialScaffold>();
        auto trans = divAsset.Transaction_Begin("UpdateTextureProperties");
        if (trans) {
            static auto diffusedims = ParameterBox::MakeParameterNameHash("diffusedims");
            static auto normaldims = ParameterBox::MakeParameterNameHash("normaldims");
            static auto paramdims = ParameterBox::MakeParameterNameHash("paramdims");
            static auto texture0 = ParameterBox::MakeParameterNameHash("texture0");
            static auto texture1 = ParameterBox::MakeParameterNameHash("texture1");
            static auto texture2 = ParameterBox::MakeParameterNameHash("texture2");
            static auto mapping0 = ParameterBox::MakeParameterNameHash("mapping0");
            static auto mapping1 = ParameterBox::MakeParameterNameHash("mapping1");
            static auto mapping2 = ParameterBox::MakeParameterNameHash("mapping2");
            static auto endheight = ParameterBox::MakeParameterNameHash("endheight");

            auto& asset = trans->GetAsset();
            asset._diffuseDims = obj._properties.GetParameter<UInt2>(diffusedims, UInt2(512, 512));
            asset._normalDims = obj._properties.GetParameter<UInt2>(normaldims, UInt2(512, 512));
            asset._paramDims = obj._properties.GetParameter<UInt2>(paramdims, UInt2(512, 512));
            asset._strata.clear();
            for (auto c=obj._children.begin(); c!=obj._children.end(); ++c) {
                auto* strataObj = sys.GetObject(obj._doc, *c);
                if (!strataObj) continue;

                TerrainMaterialScaffold::Strata newStrata;
                newStrata._texture[0] = GetRString(strataObj->_properties, texture0);
                newStrata._texture[1] = GetRString(strataObj->_properties, texture1);
                newStrata._texture[2] = GetRString(strataObj->_properties, texture2);
                newStrata._mappingConstant[0] = strataObj->_properties.GetParameter<float>(mapping0, 10.f);
                newStrata._mappingConstant[1] = strataObj->_properties.GetParameter<float>(mapping1, 10.f);
                newStrata._mappingConstant[2] = strataObj->_properties.GetParameter<float>(mapping2, 10.f);
                newStrata._endHeight = strataObj->_properties.GetParameter<float>(endheight, 1000.f);
                asset._strata.push_back(newStrata);
            }

            trans->Commit();
        }
    }

    static void TerrainBaseTextureCallback(
        const EditorDynamicInterface::FlexObjectType& flexSys, 
        EditorDynamicInterface::DocumentId doc, EditorDynamicInterface::ObjectId obj, EditorDynamicInterface::ObjectTypeId type)
    {
        auto* object = flexSys.GetObject(doc, obj);
        if (object) {
            UpdateTerrainBaseTexture(flexSys, *object);
        }
    }
 
    EditorSceneManager::EditorSceneManager()
    {
        _scene = std::make_shared<EditorScene>();
        _dynInterface = std::make_shared<EditorDynamicInterface::RegisteredTypes>();
        _dynInterface->RegisterType(std::make_shared<EditorDynamicInterface::PlacementObjectType>());
        _dynInterface->RegisterType(std::make_shared<EditorDynamicInterface::TerrainObjectType>());

        {
            using namespace EditorDynamicInterface;
            auto flexType = std::make_shared<FlexObjectType>();
            flexType->RegisterCallback(
                flexType->GetTypeId("TerrainBaseTexture"),
                &TerrainBaseTextureCallback);

            _dynInterface->RegisterType(flexType);
        }

        _selection = gcnew ObjectSet;
    }

    EditorSceneManager::~EditorSceneManager()
    {
        _scene.reset();
        _dynInterface.reset();
    }

    EditorSceneManager::!EditorSceneManager()
    {
        _scene.reset();
        _dynInterface.reset();
    }

    public ref class EditorSceneOverlay : public IOverlaySystem
    {
    public:
        void RenderToScene(
            RenderCore::IThreadContext* threadContext, 
            SceneEngine::LightingParserContext& parserContext) override;
        void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override;
        void SetActivationState(bool newState) override;

        EditorSceneOverlay(
            std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
            ObjectSet^ selection, 
            std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor);
        ~EditorSceneOverlay();
    protected:
        clix::shared_ptr<SceneEngine::ISceneParser> _sceneParser;
        ObjectSet^ _selection;
        clix::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
    };
    
    void EditorSceneOverlay::RenderToScene(
        RenderCore::IThreadContext* threadContext, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_sceneParser.get()) {
            SceneEngine::LightingParser_ExecuteScene(
                *threadContext, parserContext, *_sceneParser.get(), 
                SceneEngine::RenderingQualitySettings(threadContext->GetStateDesc()._viewportDimensions));
        }

        if (_selection) {
            // Draw a selection highlight for these items
            // at the moment, only placements can be selected... So we need to assume that 
            // they are all placements.
            auto devContext = RenderCore::Metal::DeviceContext::Get(*threadContext);
            ToolsRig::RenderHighlight(
                devContext.get(), parserContext, _placementsEditor.get(),
                (const SceneEngine::PlacementGUID*)AsPointer(_selection->_nativePlacements->cbegin()),
                (const SceneEngine::PlacementGUID*)AsPointer(_selection->_nativePlacements->cend()));
        }
    }

    void EditorSceneOverlay::RenderWidgets(
        RenderCore::IThreadContext*, 
        const RenderCore::Techniques::ProjectionDesc&)
    {}

    void EditorSceneOverlay::SetActivationState(bool) {}
    EditorSceneOverlay::EditorSceneOverlay(
        std::shared_ptr<SceneEngine::ISceneParser> sceneParser,
        ObjectSet^ selection, 
        std::shared_ptr<SceneEngine::PlacementsEditor> placementsEditor)
    {
        _sceneParser = std::move(sceneParser);
        _selection = selection;
        _placementsEditor = placementsEditor;
    }
    EditorSceneOverlay::~EditorSceneOverlay() {}

    IOverlaySystem^ EditorSceneManager::CreateOverlaySystem(VisCameraSettings^ camera)
    {
        auto sceneParser = std::make_shared<EditorSceneParser>(_scene.GetNativePtr(), camera->GetUnderlying());
        return gcnew EditorSceneOverlay(
            std::move(sceneParser), _selection, _scene->_placementsEditor);
    }

    void EditorSceneManager::SetSelection(ObjectSet^ objectSet)
    {
        *_selection->_nativePlacements = *objectSet->_nativePlacements;
        if (_scene->_placementsEditor) {
            _scene->_placementsEditor->PerformGUIDFixup(
                AsPointer(_selection->_nativePlacements->begin()),
                AsPointer(_selection->_nativePlacements->end()));
        }
    }
}
