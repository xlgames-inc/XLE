// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "ObjectPlaceholders.h"
#include "MarshalString.h"
#include "GUILayerUtil.h"
#include "IOverlaySystem.h"
#include "EditorInterfaceUtils.h"
#include "ManipulatorsLayer.h"
#include "UITypesBinding.h" // for VisCameraSettings
#include "ExportedNativeTypes.h"
#include "../EntityInterface/PlacementEntities.h"
#include "../EntityInterface/TerrainEntities.h"
#include "../EntityInterface/RetainedEntities.h"
#include "../EntityInterface/EnvironmentSettings.h"
#include "../ToolsRig/PlacementsManipulators.h"     // just needed for destructors referenced in PlacementGobInterface.h
#include "../ToolsRig/VisualisationUtils.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
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
            std::make_shared<RenderCore::Assets::ModelCache>(), Float2(0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
        _terrainManager = std::make_shared<SceneEngine::TerrainManager>(std::make_shared<SceneEngine::TerrainFormat>());
        _flexObjects = std::make_shared<EntityInterface::RetainedEntities>();
        _placeholders = std::make_shared<ObjectPlaceholders>(_flexObjects);
        _currentTime = 0.f;
    }

	EditorScene::~EditorScene()
	{}

    void EditorSceneManager::SetTypeAnnotation(
        uint typeId, String^ annotationName, 
        IEnumerable<EntityLayer::PropertyInitializer>^ initializers)
    {
        if (!_scene->_placeholders) return ;

        if (annotationName == "vis") {
            auto mappedId = _entities->GetSwitch().MapTypeId(typeId, *_flexGobInterface.get());
            if (mappedId != 0)
                _scene->_placeholders->AddAnnotation(mappedId);
        }
    }

    const EntityInterface::RetainedEntities& EditorSceneManager::GetFlexObjects()
    {
        return *_scene->_flexObjects;
    }

    IManipulatorSet^ EditorSceneManager::CreateTerrainManipulators() 
    { 
        return gcnew TerrainManipulators(_scene->_terrainManager);
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
            _scene->_terrainManager,
            _scene->_placementsEditor,
            {_scene->_placeholders->CreateIntersectionTester()} );
    }

    PlacementsEditorWrapper^ EditorSceneManager::GetPlacementsEditor()
	{
		return gcnew PlacementsEditorWrapper(_scene->_placementsEditor);
    }

    EntityLayer^ EditorSceneManager::GetEntityInterface()
    {
        return _entities;
    }

    void EditorSceneManager::IncrementTime(float increment)
    {
        _scene->IncrementTime(increment);
    }

    auto EditorSceneManager::ExportPlacements(EntityInterface::DocumentId doc, System::String^ destinationFile) -> ExportResult^
    {
            //  The document id corresponds directly with the 
            //  id used in the placements editor object. So we can
            //  just call the save function in the placements editor
            //  directly.
        auto result = gcnew ExportResult();
        result->_success = false;
        TRY
        {
            _scene->_placementsEditor->SaveCell(
                doc, clix::marshalString<clix::E_UTF8>(destinationFile).c_str());

            result->_success = true;
            result->_messages = "Success";
            return result;
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting placements: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting placements";
        } CATCH_END
        return result;
    }

    auto EditorSceneManager::ExportEnvironmentSettings(EntityInterface::DocumentId docId, System::String^ destinationFile) -> ExportResult^
    {
        auto result = gcnew ExportResult();
        result->_success = false;
        
        TRY
        {
            ExportEnvSettings(
                *_scene->_flexObjects, docId, 
                clix::marshalString<clix::E_UTF8>(destinationFile).c_str());
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting environment settings: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting environment settings";
        } CATCH_END
        return result;
    }

    auto EditorSceneManager::ExportTerrainSettings(System::String^ destinationFolder) -> ExportResult^
    {
        return nullptr;
    }

    EditorSceneManager::EditorSceneManager()
    {
        _scene = std::make_shared<EditorScene>();

        using namespace EntityInterface;
        auto placementsEditor = std::make_shared<PlacementEntities>(_scene->_placementsManager, _scene->_placementsEditor);
        auto terrainEditor = std::make_shared<TerrainEntities>(_scene->_terrainManager);
        auto flexGobInterface = std::make_shared<RetainedEntityInterface>(_scene->_flexObjects);

        auto swtch = std::make_shared<Switch>();
        swtch->RegisterType(placementsEditor);
        swtch->RegisterType(terrainEditor);
        swtch->RegisterType(flexGobInterface);
        _entities = gcnew EntityLayer(std::move(swtch));

        _flexGobInterface = flexGobInterface;
        RegisterTerrainFlexObjects(*_scene->_flexObjects);
    }

    EditorSceneManager::~EditorSceneManager()
    {
        _scene.reset();
        delete _entities;
    }

    EditorSceneManager::!EditorSceneManager() {}

    namespace Internal
    {
        IOverlaySystem^ CreateOverlaySystem(
            std::shared_ptr<EditorScene> scene, 
            std::shared_ptr<ToolsRig::VisCameraSettings> camera, 
            EditorSceneRenderSettings^ renderSettings);
    }

    IOverlaySystem^ EditorSceneManager::CreateOverlaySystem(VisCameraSettings^ camera, EditorSceneRenderSettings^ renderSettings)
    {
        return Internal::CreateOverlaySystem(
            _scene.GetNativePtr(), camera->GetUnderlying(), renderSettings);
    }
}
