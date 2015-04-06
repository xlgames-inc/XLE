// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "EditorDynamicInterface.h"
#include "PlacementsGobInterface.h"
#include "TerrainGobInterface.h"
#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../RenderCore/Techniques/TechniqueUtils.h"
#include "../../Core/Types.h"

#include "IOverlaySystem.h"
#include "UITypesBinding.h" // for VisCameraSettings
#include "../ToolsRig/VisualisationUtils.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../RenderCore/IThreadContext.h"

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
        { return (*_dynInterface)->CreateDocument(**_scene, docType, ""); }
    bool EditorSceneManager::DeleteDocument(DocumentId doc, DocumentTypeId docType)
        { return (*_dynInterface)->DeleteDocument(**_scene, doc, docType); }

    ObjectId EditorSceneManager::AssignObjectId(DocumentId doc, ObjectTypeId type)
        { return (*_dynInterface)->AssignObjectId(**_scene, doc, type); }

    bool EditorSceneManager::CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return (*_dynInterface)->CreateObject(**_scene, doc, obj, objType, ""); }

    bool EditorSceneManager::DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return (*_dynInterface)->DeleteObject(**_scene, doc, obj, objType); }

    bool EditorSceneManager::SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const void* src, size_t srcSize)
        { return (*_dynInterface)->SetProperty(**_scene, doc, obj, objType, prop, src, srcSize); }

    bool EditorSceneManager::GetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, size_t* destSize)
        { return (*_dynInterface)->GetProperty(**_scene, doc, obj, objType, prop, dest, destSize); }

    ObjectTypeId EditorSceneManager::GetTypeId(System::String^ name)                            { return (*_dynInterface)->GetTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    DocumentTypeId EditorSceneManager::GetDocumentTypeId(System::String^ name)                  { return (*_dynInterface)->GetDocumentTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    PropertyId EditorSceneManager::GetPropertyId(ObjectTypeId type, System::String^ name)       { return (*_dynInterface)->GetPropertyId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }
    ChildListId EditorSceneManager::GetChildListId(ObjectTypeId type, System::String^ name)     { return (*_dynInterface)->GetChildListId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }

    IManipulatorSet::~IManipulatorSet() {}

    ref class TerrainManipulators : public IManipulatorSet
    {
    public:
        virtual ToolsRig::IManipulator* GetManipulator(System::String^ name) override;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() override;

        TerrainManipulators(std::shared_ptr<EditorScene> scene);
        ~TerrainManipulators();
    protected:
        AutoToShared<EditorScene> _scene;
    };

	ToolsRig::IManipulator* TerrainManipulators::GetManipulator(System::String^ name)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(name);
        if ((*_scene)->_terrainGob) {
		    for (auto i : (*_scene)->_terrainGob->_terrainManipulators)
			    if (i._name == nativeName) return i._manipulator.get();
        }
		return nullptr;
	}

	System::Collections::Generic::IEnumerable<System::String^>^ TerrainManipulators::GetManipulatorNames()
	{
		auto result = gcnew System::Collections::Generic::List<System::String^>();
        if ((*_scene)->_terrainGob) {
		    for (auto i : (*_scene)->_terrainGob->_terrainManipulators)
			    result->Add(clix::marshalString<clix::E_UTF8>(i._name));
        }
		return result;
	}

    TerrainManipulators::TerrainManipulators(std::shared_ptr<EditorScene> scene)
    {
        _scene.reset(new std::shared_ptr<EditorScene>(std::move(scene)));
    }

    TerrainManipulators::~TerrainManipulators() {}

    IManipulatorSet^ EditorSceneManager::GetTerrainManipulators() { return _terrainManipulators; }

	IntersectionTestSceneWrapper^ EditorSceneManager::GetIntersectionScene()
	{
		auto native = std::make_shared<SceneEngine::IntersectionTestScene>(
            ((*_scene)->_terrainGob) ? (*_scene)->_terrainGob->_terrainManager : nullptr,
            (*_scene)->_placementsEditor);
		return gcnew IntersectionTestSceneWrapper(native);
	}
 
    EditorSceneManager::EditorSceneManager()
    {
        InitAutoToShared(_scene);
        InitAutoToShared(_dynInterface);
        (*_dynInterface)->RegisterType(std::make_shared<EditorDynamicInterface::PlacementObjectType>());
        (*_dynInterface)->RegisterType(std::make_shared<EditorDynamicInterface::TerrainObjectType>());
        _terrainManipulators = gcnew TerrainManipulators(*_scene);
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

        EditorSceneOverlay(std::shared_ptr<SceneEngine::ISceneParser> sceneParser);
        ~EditorSceneOverlay();
    protected:
        AutoToShared<SceneEngine::ISceneParser> _sceneParser;
    };
    
    void EditorSceneOverlay::RenderToScene(
        RenderCore::IThreadContext* threadContext, 
        SceneEngine::LightingParserContext& parserContext)
    {
        if (_sceneParser.get()) {
            SceneEngine::LightingParser_ExecuteScene(
                *threadContext, parserContext, **_sceneParser, 
                SceneEngine::RenderingQualitySettings(threadContext->GetStateDesc()._viewportDimensions));
        }
    }

    void EditorSceneOverlay::RenderWidgets(
        RenderCore::IThreadContext*, 
        const RenderCore::Techniques::ProjectionDesc&)
    {}

    void EditorSceneOverlay::SetActivationState(bool) {}
    EditorSceneOverlay::EditorSceneOverlay(std::shared_ptr<SceneEngine::ISceneParser> sceneParser)
    {
        _sceneParser.reset(new std::shared_ptr<SceneEngine::ISceneParser>(std::move(sceneParser)));
    }
    EditorSceneOverlay::~EditorSceneOverlay() {}

    IOverlaySystem^ EditorSceneManager::CreateOverlaySystem(VisCameraSettings^ camera)
    {
        auto sceneParser = std::make_shared<EditorSceneParser>(*_scene, camera->GetUnderlying());
        return gcnew EditorSceneOverlay(std::move(sceneParser));
    }
}
