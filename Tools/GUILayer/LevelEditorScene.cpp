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
#include "TerrainLayer.h"
#include "UITypesBinding.h" // for VisCameraSettings
#include "Exceptions.h"
#include "ExportedNativeTypes.h"
#include "../EntityInterface/PlacementEntities.h"
#include "../EntityInterface/TerrainEntities.h"
#include "../EntityInterface/RetainedEntities.h"
#include "../EntityInterface/EnvironmentSettings.h"
#include "../EntityInterface/VegetationSpawnEntities.h"
#include "../EntityInterface/GameObjects.h"
#include "../ToolsRig/PlacementsManipulators.h"     // just needed for destructors referenced in PlacementGobInterface.h
#include "../ToolsRig/TerrainManipulators.h"        // for TerrainManipulatorContext
#include "../ToolsRig/VisualisationUtils.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../SceneEngine/VegetationSpawn.h"
#include "../../SceneEngine/VolumetricFog.h"
#include "../../SceneEngine/ShallowSurface.h"
#include "../../SceneEngine/DynamicImposters.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../Utility/Streams/StreamTypes.h"
#include "../../Utility/Streams/PathUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/Streams/StreamFormatter.h"
#include "../../Utility/Conversion.h"
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
            SceneEngine::WorldPlacementsConfig(), modelCache, Float3(0.f, 0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
            // note --  we need to have the terrain manager a default terrain format here... But it's too early
            //          for some settings (like the gradient flags settings!)
        auto defTerrainFormat = std::make_shared<SceneEngine::TerrainFormat>(SceneEngine::GradientFlagsSettings(true));
        _terrainManager = std::make_shared<SceneEngine::TerrainManager>(defTerrainFormat);
        _vegetationSpawnManager = std::make_shared<SceneEngine::VegetationSpawnManager>(modelCache);
        _volumeFogManager = std::make_shared<SceneEngine::VolumetricFogManager>();
        _shallowSurfaceManager = std::make_shared<SceneEngine::ShallowSurfaceManager>();
        _flexObjects = std::make_shared<EntityInterface::RetainedEntities>();
        _placeholders = std::make_shared<ObjectPlaceholders>(_flexObjects);
        _dynamicImposters = std::make_shared<SceneEngine::DynamicImposters>(modelCache->GetSharedStateSet());
        _placementsManager->SetImposters(_dynamicImposters);
        _currentTime = 0.f;
    }

	EditorScene::~EditorScene()
	{}

    template <typename Type>
        Type GetFirst(IEnumerable<Type>^ input)
        {
            if (!input) return Type();
            auto e = input->GetEnumerator();
            if (!e || !e->MoveNext()) return Type();
            return e->Current;
        }

    void EditorSceneManager::SetTypeAnnotation(
        uint typeId, String^ annotationName, 
        IEnumerable<EntityLayer::PropertyInitializer>^ initializers)
    {
        if (!_scene->_placeholders) return ;

        if (annotationName == "vis") {
            auto mappedId = _entities->GetSwitch().MapTypeId(typeId, *_flexGobInterface.get());
            if (mappedId != 0) {
                std::string geoType;
                auto p = GetFirst(initializers);
                if (p._src)
                    geoType = Conversion::Convert<std::string>(
                        std::basic_string<utf16>((const utf16*)p._src, &((const utf16*)p._src)[p._arrayCount]));
                
                _scene->_placeholders->AddAnnotation(mappedId, geoType);
            }
        }
    }

    const EntityInterface::RetainedEntities& EditorSceneManager::GetFlexObjects()
    {
        return *_scene->_flexObjects;
    }

    void EditorSceneManager::SaveTerrainLock(uint layerId)
    {
        if (!_scene->_terrainManager) return;

        SceneEngine::GenericUberSurfaceInterface* interf = nullptr;
        if (layerId == SceneEngine::CoverageId_Heights) {
            interf = _scene->_terrainManager->GetHeightsInterface();
        } else {
            interf = _scene->_terrainManager->GetCoverageInterface(layerId);
        }
        if (!interf) return;

        TRY { interf->FlushLockToDisk(); } 
        CATCH (const std::exception& e) { Throw(Marshal(e)); }
        CATCH_END
    }

    void EditorSceneManager::AbandonTerrainLock(uint layerId)
    {
        if (!_scene->_terrainManager) return;

        SceneEngine::GenericUberSurfaceInterface* interf = nullptr;
        if (layerId == SceneEngine::CoverageId_Heights) {
            interf = _scene->_terrainManager->GetHeightsInterface();
        } else {
            interf = _scene->_terrainManager->GetCoverageInterface(layerId);
        }
        if (!interf) return;

        TRY { interf->AbandonLock(); }
        CATCH (const std::exception& e) { Throw(Marshal(e)); }
        CATCH_END
    }

    static bool HasLock(const SceneEngine::GenericUberSurfaceInterface& interf)
    {
        auto lock = interf.GetLock();
        return (lock.first[0] < lock.second[0]) && (lock.first[1] < lock.second[1]);
    }

    bool EditorSceneManager::HasTerrainLock(uint layerId)
    {
        if (!_scene->_terrainManager) return false;

        SceneEngine::GenericUberSurfaceInterface* interf = nullptr;
        if (layerId == SceneEngine::CoverageId_Heights) {
            interf = _scene->_terrainManager->GetHeightsInterface();
        } else {
            interf = _scene->_terrainManager->GetCoverageInterface(layerId);
        }
        return interf && HasLock(*interf);
    }

    IManipulatorSet^ EditorSceneManager::CreateTerrainManipulators(TerrainManipulatorContext^ context) 
    { 
        return gcnew TerrainManipulators(_scene->_terrainManager, context->GetNative());
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

    ref class TextPendingExport : public EditorSceneManager::PendingExport
    {
    public:
        EditorSceneManager::ExportResult PerformExport(System::String^ destFile) override
        {
            EditorSceneManager::ExportResult result;
            TRY
            {
                auto nativeDestFile = clix::marshalString<clix::E_UTF8>(destFile);
                PrepareDirectoryForFile(nativeDestFile);

                auto output = OpenFileOutput(nativeDestFile.c_str(), "wb");
                // pin_ptr<const wchar_t> p = ::PtrToStringChars(_preview);
                // output->Write(p, _preview->Length * sizeof(wchar_t));
                    // we need to use clix::marshalString in order to convert to UTF8 characters
                auto nativeString = clix::marshalString<clix::E_UTF8>(_preview);
                output->Write(AsPointer(nativeString.cbegin()), int(nativeString.size() * sizeof(decltype(nativeString)::value_type)));
                
                result._success = true;
                result._messages = "Success";
                
            } CATCH(const std::exception& e) {
                result._messages = "Error while writing to file: " + destFile + " : " + clix::marshalString<clix::E_UTF8>(e.what());
            } CATCH(...) {
                result._messages = "Unknown error occurred while writing to file " + destFile;
            } CATCH_END
            return result;
        }
    };

	enum class StreamWriterResult { Text, Binary, MetricsText, None };

	static auto __clrcall AsPendingExportType(StreamWriterResult input) -> EditorSceneManager::PendingExport::Type
	{
		switch (input) {
		case StreamWriterResult::Text: return EditorSceneManager::PendingExport::Type::Text;
		case StreamWriterResult::Binary: return EditorSceneManager::PendingExport::Type::Binary;
		case StreamWriterResult::MetricsText: return EditorSceneManager::PendingExport::Type::MetricsText;
		default:
		case StreamWriterResult::None: return EditorSceneManager::PendingExport::Type::None;
		}	
	}

    template<typename Type>
        static EditorSceneManager::PendingExport^ ExportViaStream(
            System::String^ typeName,
            Type streamWriter)
    {
        auto result = gcnew TextPendingExport();
        result->_success = false;
        
        TRY
        {
            MemoryOutputStream<utf8> stream;
            result->_previewType = AsPendingExportType(streamWriter(stream));
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

    static auto WriteEnvSettings(OutputStream& stream, uint64 docId, EntityInterface::RetainedEntities* flexObjects) 
        -> StreamWriterResult
    {
        OutputStreamFormatter formatter(stream);
        EntityInterface::ExportEnvSettings(formatter, *flexObjects, docId);
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportEnv(EntityInterface::DocumentId docId) -> PendingExport^
    {
        return ExportViaStream(
            "environment settings",
            std::bind(WriteEnvSettings, _1, docId, _scene->_flexObjects.get()));
    }

    static auto WriteGameObjects(
        OutputStream& stream, uint64 docId, 
        EntityInterface::RetainedEntities* flexObjects) -> StreamWriterResult
    {
        OutputStreamFormatter formatter(stream);
        EntityInterface::ExportGameObjects(formatter, *flexObjects, docId);
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportGameObjects(
        EntityInterface::DocumentId docId) -> PendingExport^
    {
        return ExportViaStream(
            "game objects",
            std::bind(WriteGameObjects, _1, docId, _scene->_flexObjects.get()));
    }

    static auto WritePlacementsCfg(
        OutputStream& stream, 
        IEnumerable<EditorSceneManager::PlacementCellRef>^ cells) -> StreamWriterResult
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
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportPlacementsCfg(IEnumerable<PlacementCellRef>^ cells) -> PendingExport^
    {
        return ExportViaStream(
            "placements config",
            std::bind(WritePlacementsCfg, _1, gcroot<IEnumerable<PlacementCellRef>^>(cells)));
    }

    ref class PlacementsPendingExport : public EditorSceneManager::PendingExport
    {
    public:
        EditorSceneManager::ExportResult PerformExport(System::String^ destFile) override
        {
            EditorSceneManager::ExportResult result;
            TRY
            {
                auto nativeDestFile = clix::marshalString<clix::E_UTF8>(destFile);
                PrepareDirectoryForFile(nativeDestFile);

                _placements->WriteCell(_doc, nativeDestFile.c_str());

                    // write metrics file as well
                {
                    auto metrics = _placements->GetMetricsString(_doc);
                    auto output = OpenFileOutput((nativeDestFile + ".metrics").c_str(), "wb");
                    output->Write(MakeStringSection(Conversion::Convert<std::basic_string<utf8>>(metrics)));
                }

                result._success = true;
                result._messages = "Success";
                
            } CATCH(const std::exception& e) {
                result._messages = "Error while writing to file: " + destFile + " : " + clix::marshalString<clix::E_UTF8>(e.what());
            } CATCH(...) {
                result._messages = "Unknown error occurred while writing to file " + destFile;
            } CATCH_END
            return result;
        }

        PlacementsPendingExport(
            EntityInterface::DocumentId doc,
            std::shared_ptr<SceneEngine::PlacementsEditor> placements) : _doc(doc), _placements(placements) {}
    private:
        EntityInterface::DocumentId _doc;
        clix::shared_ptr<SceneEngine::PlacementsEditor> _placements;
    };

    auto EditorSceneManager::ExportPlacements(EntityInterface::DocumentId placementsDoc) -> PendingExport^
    {
        auto result = gcnew PlacementsPendingExport(placementsDoc, _scene->_placementsEditor);
        result->_success = false;

        TRY
        {
            result->_preview = clix::marshalString<clix::E_UTF8>(
                _scene->_placementsEditor->GetMetricsString(placementsDoc));
            result->_success = true;
            result->_messages = "Success";
            result->_previewType = PendingExport::Type::MetricsText;
        } CATCH(const std::exception& e) {
            result->_messages = "Error while exporting placements: " + clix::marshalString<clix::E_UTF8>(e.what());
        } CATCH(...) {
            result->_messages = "Unknown error occurred while exporting placements";
        } CATCH_END

        return result;
    }

    static auto WriteTerrainCfg(OutputStream& stream, SceneEngine::TerrainConfig& cfg) 
        -> StreamWriterResult
    {
        OutputStreamFormatter formatter(stream);
        cfg.Write(formatter);
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportTerrain(TerrainConfig^ cfg) -> PendingExport^
    {
        return ExportViaStream("terrain", std::bind(WriteTerrainCfg, _1, cfg->GetNative()));
    }

    static auto WriteTerrainCachedData(OutputStream& stream, SceneEngine::TerrainManager* terrainMan) 
        -> StreamWriterResult
    {
        SceneEngine::WriteTerrainCachedData(stream, terrainMan->GetConfig(), *terrainMan->GetFormat());
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportTerrainCachedData() -> PendingExport^
    {
        return ExportViaStream(
            "terrain cached data",
            std::bind(WriteTerrainCachedData, _1, _scene->_terrainManager.get()));
    }

    static auto WriteTerrainMaterialData(OutputStream& stream, SceneEngine::TerrainManager* terrainMan) 
        -> StreamWriterResult
    {
        SceneEngine::WriteTerrainMaterialData(stream, terrainMan->GetMaterialConfig());
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportTerrainMaterialData() -> PendingExport^
    {
        return ExportViaStream(
            "terrain material data",
            std::bind(WriteTerrainMaterialData, _1, _scene->_terrainManager.get()));
    }

    static auto WriteVegetationSpawnConfig(OutputStream& stream, SceneEngine::VegetationSpawnManager* man) 
        -> StreamWriterResult
    {
        Utility::OutputStreamFormatter formatter(stream);
        man->GetConfig().Write(formatter);
        return StreamWriterResult::Text;
    }

    auto EditorSceneManager::ExportVegetationSpawn(EntityInterface::DocumentId docId) -> PendingExport^
    {
        return ExportViaStream(
            "vegetation spawn",
            std::bind(WriteVegetationSpawnConfig, _1, _scene->_vegetationSpawnManager.get()));
    }

    void EditorSceneManager::UnloadTerrain()
    {
        // Check for active "locks" before we unload. If we find any, throw an exception back
        bool hasLock = false;
        auto* heightsIntrf = _scene->_terrainManager->GetHeightsInterface();
        hasLock |= (heightsIntrf && HasLock(*heightsIntrf));
        auto cfg = _scene->_terrainManager->GetConfig();
        for (unsigned l=0; l<cfg.GetCoverageLayerCount(); ++l) {
            auto * intrf = _scene->_terrainManager->GetCoverageInterface(cfg.GetCoverageLayer(l)._id);
            hasLock |= (intrf && HasLock(*intrf));
        }
        if (hasLock)
            throw gcnew System::Exception("Cannot unload the terrain because there are active terrain locks. Save or abandon the terrain lock before performing this operation.");

		_scene->_terrainManager->Reset();
    }

    void EditorSceneManager::ReloadTerrain(TerrainConfig^ cfg)
    {
        auto nativeCfg = cfg->GetNative();
        _scene->_terrainManager->Load(nativeCfg, Int2(0,0), nativeCfg._cellCount, true);
        _terrainInterface->OnTerrainReload();
        EntityInterface::ReloadTerrainFlexObjects(*_scene->_flexObjects, *_scene->_terrainManager);
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
        RegisterTerrainFlexObjects(*_scene->_flexObjects, _scene->_terrainManager);
        RegisterVegetationSpawnFlexObjects(*_scene->_flexObjects, _scene->_vegetationSpawnManager);

        RegisterDynamicImpostersFlexObjects(*_scene->_flexObjects, _scene->_dynamicImposters);

        auto envEntitiesManager = std::make_shared<::EntityInterface::EnvEntitiesManager>(_scene->_flexObjects);
        envEntitiesManager->RegisterVolumetricFogFlexObjects(_scene->_volumeFogManager);
        envEntitiesManager->RegisterEnvironmentFlexObjects();
        envEntitiesManager->RegisterShallowSurfaceFlexObjects(_scene->_shallowSurfaceManager);
        _envEntitiesManager = envEntitiesManager;

        _scene->_prepareSteps.push_back(
            std::bind(&::EntityInterface::EnvEntitiesManager::FlushUpdates, _envEntitiesManager.get()));
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
