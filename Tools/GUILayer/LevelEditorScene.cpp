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
#include "../EntityInterface/VegetationSpawnEntities.h"
#include "../ToolsRig/PlacementsManipulators.h"     // just needed for destructors referenced in PlacementGobInterface.h
#include "../ToolsRig/VisualisationUtils.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../Utility/Streams/Data.h"
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include <memory>

namespace GUILayer
{
    using namespace std::placeholders;

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
        auto modelCache = std::make_shared<RenderCore::Assets::ModelCache>();
        _placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
            SceneEngine::WorldPlacementsConfig(), modelCache, Float2(0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
        _terrainManager = std::make_shared<SceneEngine::TerrainManager>(std::make_shared<SceneEngine::TerrainFormat>());
        _vegetationSpawnManager = std::make_shared<SceneEngine::VegetationSpawnManager>(modelCache);
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

    EditorScene& EditorSceneManager::GetScene()
    {
        return *_scene.get();
    }

    static void PrepareDirectoryForFile(std::string& destinationFile)
    {
        utf8 dirName[MaxPath];
        XlDirname((char*)dirName, dimof(dirName), destinationFile.c_str());
        CreateDirectoryRecursive((char*)dirName);
    }

    template<typename Type>
        static EditorSceneManager::ExportResult^ ExportViaStream(
            System::String^ typeName, System::String^ destinationFile,
            Type streamWriter)
    {
        auto result = gcnew EditorSceneManager::ExportResult();
        result->_success = false;
        
        TRY
        {
                // attempt to create the directory, if we need to
            auto nativeDestFile = clix::marshalString<clix::E_UTF8>(destinationFile);
            PrepareDirectoryForFile(nativeDestFile);

            {
                    // write out to a file stream
                auto output = OpenFileOutput(nativeDestFile.c_str(), "wb");
                streamWriter(*output);
            }

            result->_success = true;
            result->_messages = "Success";
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting " + typeName +": " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting " + typeName;
        } CATCH_END
        return result;
    }

    template<typename Type>
        static EditorSceneManager::ExportPreview^ PreviewViaStream(
            System::String^ typeName,
            Type streamWriter)
    {
        auto result = gcnew EditorSceneManager::ExportPreview();
        result->_success = false;
        
        TRY
        {
            MemoryOutputStream<utf8> stream;
            result->_type = streamWriter(stream);

            result->_preview = clix::marshalString<clix::E_UTF8>(stream.GetBuffer().str());
            result->_success = true;
            result->_messages = "Success";
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting environment settings: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting environment settings";
        } CATCH_END
        return result;
    }

    static auto WriteEnvSettings(OutputStream& stream, uint64 docId, EntityInterface::RetainedEntities* flexObjects) -> EditorSceneManager::ExportPreview::Type
    {
        OutputStreamFormatter formatter(stream);
        EntityInterface::ExportEnvSettings(formatter, *flexObjects, docId);
        return EditorSceneManager::ExportPreview::Type::Text;
    }

    auto EditorSceneManager::ExportEnv(EntityInterface::DocumentId docId, System::String^ destinationFile) -> ExportResult^
    {
        return ExportViaStream(
            "environment settings", destinationFile,
            std::bind(WriteEnvSettings, _1, docId, _scene->_flexObjects.get()));
    }

    auto EditorSceneManager::PreviewExportEnv(EntityInterface::DocumentId docId) -> ExportPreview^
    {
        auto result = PreviewViaStream(
            "environment settings",
            std::bind(WriteEnvSettings, _1, docId, _scene->_flexObjects.get()));

        // {
        //     auto nativeString = clix::marshalString<clix::E_UTF8>(result->_preview);
        //     MemoryMappedInputStream str(AsPointer(nativeString.cbegin()), AsPointer(nativeString.cend()));
        //     InputStreamFormatter<utf8> formatter(str);
        //     auto envTest = EntityInterface::DeserializeEnvSettings(formatter);
        //     (void)envTest;
        // }

        return result;
    }

    static auto WriteTerrainCachedData(OutputStream& stream, SceneEngine::TerrainManager* terrainMan) -> EditorSceneManager::ExportPreview::Type
    {
        SceneEngine::WriteTerrainCachedData(stream, terrainMan->GetConfig(), *terrainMan->GetFormat());
        return EditorSceneManager::ExportPreview::Type::Text;
    }

    auto EditorSceneManager::ExportTerrainCachedData(System::String^ destinationFile) -> ExportResult^
    {
        return ExportViaStream(
            "terrain cached data", destinationFile,
            std::bind(WriteTerrainCachedData, _1, _scene->_terrainManager.get()));
    }

    auto EditorSceneManager::PreviewExportTerrainCachedData() -> ExportPreview^
    {
        return PreviewViaStream(
            "terrain cached data",
            std::bind(WriteTerrainCachedData, _1, _scene->_terrainManager.get()));
    }

    static auto WriteTerrainMaterialData(OutputStream& stream, SceneEngine::TerrainManager* terrainMan) -> EditorSceneManager::ExportPreview::Type
    {
        SceneEngine::WriteTerrainMaterialData(stream, terrainMan->GetConfig());
        return EditorSceneManager::ExportPreview::Type::Text;
    }

    auto EditorSceneManager::ExportTerrainMaterialData(System::String^ destinationFile) -> ExportResult^
    {
        return ExportViaStream(
            "terrain material data", destinationFile,
            std::bind(WriteTerrainMaterialData, _1, _scene->_terrainManager.get()));
    }

    auto EditorSceneManager::PreviewExportTerrainMaterialData() -> ExportPreview^
    {
        return PreviewViaStream(
            "terrain material data",
            std::bind(WriteTerrainMaterialData, _1, _scene->_terrainManager.get()));
    }

    static auto WritePlacementsCfg(
        OutputStream& stream, 
        IEnumerable<EditorSceneManager::PlacementCellRef>^ cells) -> EditorSceneManager::ExportPreview::Type
    {
        OutputStreamFormatter formatter(stream);
        for each(auto c in cells) {
            auto ele = formatter.BeginElement("CellRef");
            formatter.WriteAttribute("Offset", ImpliedTyping::AsString(AsFloat3(c.Offset)));
            formatter.WriteAttribute("Mins", ImpliedTyping::AsString(AsFloat3(c.Mins)));
            formatter.WriteAttribute("Maxs", ImpliedTyping::AsString(AsFloat3(c.Maxs)));
            formatter.WriteAttribute("NativeFile", clix::marshalString<clix::E_UTF8>(c.NativeFile));
            formatter.EndElement(ele);
        }
        formatter.Flush();
        return EditorSceneManager::ExportPreview::Type::Text;
    }

    auto EditorSceneManager::PreviewExportPlacementsCfg(IEnumerable<PlacementCellRef>^ cells) -> ExportPreview^
    {
        return PreviewViaStream(
            "placements config",
            std::bind(WritePlacementsCfg, _1, gcroot<IEnumerable<PlacementCellRef>^>(cells)));
    }

    auto EditorSceneManager::ExportPlacementsCfg(IEnumerable<PlacementCellRef>^ cells, System::String^ destinationFile) -> ExportResult^
    {
        return ExportViaStream(
            "placements config", destinationFile,
            std::bind(WritePlacementsCfg, _1, gcroot<IEnumerable<PlacementCellRef>^>(cells)));
    }

    auto EditorSceneManager::ExportPlacements(EntityInterface::DocumentId doc, System::String^ destinationFile) -> ExportResult^
    {
            //  The document id corresponds directly with the 
            //  id used in the placements editor object. So we can
            //  just call the save function in the placements editor
            //  directly.
        auto result = gcnew EditorSceneManager::ExportResult();
        result->_success = false;
        
        TRY
        {
                // attempt to create the directory, if we need to
            auto nativeDestFile = clix::marshalString<clix::E_UTF8>(destinationFile);
            PrepareDirectoryForFile(nativeDestFile);
            _scene->_placementsEditor->WriteCell(doc, nativeDestFile.c_str());

                // write metrics file as well
            {
                auto metrics = _scene->_placementsEditor->GetMetricsString(doc);
                auto output = OpenFileOutput((nativeDestFile + ".metrics").c_str(), "wb");
                output->WriteString((const utf8*)AsPointer(metrics.cbegin()), (const utf8*)AsPointer(metrics.cend()));
            }

            result->_success = true;
            result->_messages = "Success";
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting placements: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting placements";
        } CATCH_END
        return result;
    }

    auto EditorSceneManager::PreviewExportPlacements(EntityInterface::DocumentId placementsDoc) -> ExportPreview^
    {
        auto result = gcnew ExportPreview();
        result->_success = false;

        TRY
        {
            result->_preview = clix::marshalString<clix::E_UTF8>(
                _scene->_placementsEditor->GetMetricsString(placementsDoc));
            result->_success = true;
            result->_messages = "Success";
            result->_type = ExportPreview::Type::MetricsText;
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting placements: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting placements";
        } CATCH_END

        return result;
    }

    auto EditorSceneManager::ExportTerrainSettings(System::String^ destinationFolder) -> ExportResult^
    {
        return nullptr;
    }

    void EditorSceneManager::UnloadTerrain()
    {
        _terrainInterface->UnloadTerrain();
    }

    void EditorSceneManager::ReloadTerrain()
    {
        _terrainInterface->ReloadTerrain();
        EntityInterface::ReloadTerrainFlexObjects(*_scene->_flexObjects);
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
        _terrainInterface = terrainEditor;
        RegisterTerrainFlexObjects(*_scene->_flexObjects);
        RegisterVegetationSpawnFlexObjects(*_scene->_flexObjects, _scene->_vegetationSpawnManager);
        RegisterEnvironmentFlexObjects(*_scene->_flexObjects);
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
