// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EditorInterfaceUtils.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "LevelEditorScene.h"
#include "TerrainLayer.h"
#include "GUILayerUtil.h"
#include "MathLayer.h"
#include "MarshalString.h"
#include "ExportedNativeTypes.h"
#include "../EntityInterface/EntityInterface.h"
#include "../EntityInterface/EnvironmentSettings.h"
#include "../ToolsRig/VisualisationUtils.h"
#include "../ToolsRig/TerrainConversion.h"
#include "../../PlatformRig/BasicSceneParser.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/IDevice.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderOverlays/DebuggingDisplay.h"
#include "../../Utility/StringUtils.h"
#include "../../Assets/Assets.h"
#include "../../Math/Transformations.h"
#include "../ToolsRig/PlacementsManipulators.h"
// #include "../../ConsoleRig/Log.h"        (can't include in Win32 managed code)

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

extern "C" __declspec(dllimport) short __stdcall GetKeyState(int nVirtKey);

namespace GUILayer
{
    public value class HitRecord
    {
    public:
        EntityInterface::DocumentId _document;
        EntityInterface::ObjectId _object;
        Vector3 _worldSpaceCollision;
        float _distance;
        uint64 _materialGuid;
        String^ _materialName;
        String^ _modelName;
        unsigned _drawCallIndex;
    };

    void ObjectSet::Add(uint64 document, uint64 id)
    {
        _nativePlacements->push_back(std::make_pair(document, id));
    }

    void ObjectSet::Clear() { _nativePlacements->clear(); }
    bool ObjectSet::IsEmpty() { return _nativePlacements->empty(); }

    void ObjectSet::DoFixup(SceneEngine::PlacementsEditor& placements)
    {
        placements.PerformGUIDFixup(
            AsPointer(_nativePlacements->begin()),
            AsPointer(_nativePlacements->end()));
    }
        
    void ObjectSet::DoFixup(PlacementsEditorWrapper^ placements)
    {
        DoFixup(placements->GetNative());
    }

    ObjectSet::ObjectSet()
    { 
        _nativePlacements.reset(new NativePlacementSet); 
    }
    ObjectSet::~ObjectSet()    { _nativePlacements.reset(); }

    public ref class CameraDescWrapper
    {
    public:
        CameraDescWrapper(ToolsRig::VisCameraSettings& camSettings)
        {
            _native = new RenderCore::Techniques::CameraDesc(
                ToolsRig::AsCameraDesc(camSettings));
        }
        ~CameraDescWrapper() { this->!CameraDescWrapper();}
        !CameraDescWrapper() { delete _native; }

        RenderCore::Techniques::CameraDesc* _native;
    };

    public ref class EditorInterfaceUtils
    {
    public:
        static void* CalculateWorldToProjection(
            CameraDescWrapper^ camera, float viewportAspect)
        {
            auto proj = RenderCore::Techniques::PerspectiveProjection(
                *camera->_native, viewportAspect);
            auto temp = std::make_unique<Float4x4>(Combine(
                InvertOrthonormalTransform(camera->_native->_cameraToWorld), proj));
            return temp.release();
        }

        static IntersectionTestContextWrapper^
            CreateIntersectionTestContext(
                EngineDevice^ engineDevice,
                TechniqueContextWrapper^ techniqueContext,
                CameraDescWrapper^ camera,
                unsigned viewportWidth, unsigned viewportHeight)
        {
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> nativeTC;
            if (techniqueContext) {
                nativeTC = techniqueContext->_techniqueContext.GetNativePtr();
            } else {
                nativeTC = std::make_shared<RenderCore::Techniques::TechniqueContext>();
            }
            return gcnew IntersectionTestContextWrapper(
                std::make_shared<SceneEngine::IntersectionTestContext>(
                    engineDevice->GetNative().GetRenderDevice()->GetImmediateContext(),
                    *camera->_native,
                    std::make_shared<RenderCore::ViewportContext>(UInt2(viewportWidth, viewportHeight)),
                    nativeTC));
        }

        static System::Collections::Generic::ICollection<HitRecord>^
            RayIntersection(
                IntersectionTestSceneWrapper^ testScene,
                IntersectionTestContextWrapper^ testContext,
                Vector3 start, Vector3 end, unsigned filter)
        {
            TRY
            {
                auto firstResult = testScene->_scene->FirstRayIntersection(
                    *testContext->_context.get(),
                    std::make_pair(AsFloat3(start), AsFloat3(end)),
                    filter);

                if (firstResult._type != 0) {
                    auto record = gcnew HitRecord;
                    record->_document = firstResult._objectGuid.first;
                    record->_object = firstResult._objectGuid.second;
                    record->_worldSpaceCollision = AsVector3(firstResult._worldSpaceCollision);
                    record->_distance = firstResult._distance;
                    record->_materialGuid = firstResult._materialGuid;
                    record->_materialName = clix::marshalString<clix::E_UTF8>(firstResult._materialName);
                    record->_modelName = clix::marshalString<clix::E_UTF8>(firstResult._modelName);
                    record->_drawCallIndex = firstResult._drawCallIndex;

                        // hack -- for placement objects, we must strip off the top 32 bits
                        //          from the object id.
                    if (firstResult._type == SceneEngine::IntersectionTestScene::Type::Placement) {
                        record->_object &= 0x00000000ffffffffull;
                    }

                    auto result = gcnew System::Collections::Generic::List<HitRecord>();
                    result->Add(*record);
                    return result;
                }
            }
            CATCH(const ::Assets::Exceptions::InvalidResource& e)
            {
                (void)e;
                // LogWarning << "Invalid resource while performing ray intersection test: {" << e.what() << "}";
            }
            CATCH(const ::Assets::Exceptions::PendingResource&) {}
            CATCH_END

            return nullptr;
        }

        static System::Collections::Generic::ICollection<HitRecord>^
            FrustumIntersection(
                IntersectionTestSceneWrapper^ testScene,
                IntersectionTestContextWrapper^ testContext,
                const float matrix[],
                unsigned filter)
        {
            TRY
            {
                Float4x4 worldToProjection = Transpose(AsFloat4x4(matrix));

                auto nativeResults = testScene->_scene->FrustumIntersection(
                    *testContext->_context.get(),
                    worldToProjection, filter);

                if (!nativeResults.empty()) {

                    auto result = gcnew System::Collections::Generic::List<HitRecord>();
                    for (const auto& i: nativeResults) {
                        auto record = gcnew HitRecord;
                        record->_document = i._objectGuid.first;
                        record->_object = i._objectGuid.second;
                        record->_distance = i._distance;
                        record->_worldSpaceCollision = AsVector3(i._worldSpaceCollision);
                        record->_drawCallIndex = (unsigned)-1;

                            // hack -- for placement objects, we must strip off the top 32 bits
                            //          from the object id.
                        if (i._type == SceneEngine::IntersectionTestScene::Type::Placement) {
                            record->_object &= 0x00000000ffffffffull;
                        }
                    
                        result->Add(*record);
                    }

                    return result;
                }
            }
            CATCH(const ::Assets::Exceptions::InvalidResource& e)
            {
                (void)e;
                // LogWarning << "Invalid resource while performing ray intersection test: {" << e.what() << "}";
            }
            CATCH(const ::Assets::Exceptions::PendingResource&) {}
            CATCH_END

            return nullptr;
        }

        static bool GetTerrainHeight(
            [Out] float% result,
            IntersectionTestSceneWrapper^ testScene,
            float worldX, float worldY)
        {
            auto& terrain = *testScene->GetNative().GetTerrain();
            result = SceneEngine::GetTerrainHeight(
                *terrain.GetFormat().get(), terrain.GetConfig(), terrain.GetCoords(),
                Float2(worldX, worldY));
            return true;
        }

        static bool GetTerrainUnderCursor(
            [Out] Vector3% intersection,
            IntersectionTestContextWrapper^ context,
            IntersectionTestSceneWrapper^ testScene,
            unsigned ptx, unsigned pty)
        {
            auto test = testScene->_scene->UnderCursor(
                *context->_context.get(), UInt2(ptx, pty),
                SceneEngine::IntersectionTestScene::Type::Terrain);
            if (test._type == SceneEngine::IntersectionTestScene::Type::Terrain) {
                intersection = AsVector3(test._worldSpaceCollision);
                return true;
            }
            return false;
        }

        ref class ScatterPlaceOperation
        {
        public:
            List<Tuple<uint64, uint64>^>^ _toBeDeleted;
            List<Vector3>^ _creationPositions;
        };

        static ScatterPlaceOperation^ CalculateScatterOperation(
            PlacementsEditorWrapper^ placements,
            IntersectionTestSceneWrapper^ scene,
            System::String^ modelName, 
            Vector3 centre, float radius, float density)
        {
            std::vector<SceneEngine::PlacementGUID> toBeDeleted;
            std::vector<Float3> spawnPositions;

            ToolsRig::CalculateScatterOperation(
                toBeDeleted, spawnPositions,
                placements->GetNative(), scene->GetNative(), 
                clix::marshalString<clix::E_UTF8>(modelName).c_str(),
                AsFloat3(centre), radius, density);

            auto result = gcnew ScatterPlaceOperation();
            result->_toBeDeleted = gcnew List<Tuple<uint64, uint64>^>();
            result->_creationPositions = gcnew List<Vector3>();
            for (const auto& d:toBeDeleted) result->_toBeDeleted->Add(Tuple::Create(d.first, d.second & 0xffffffffull));
            for (const auto& p:spawnPositions) result->_creationPositions->Add(Vector3(p[0], p[1], p[2]));

            return result;
        }
        
        static unsigned AsTypeId(System::Type^ type)
        {
            if (type == bool::typeid)                   { return (unsigned)ImpliedTyping::TypeCat::Bool; }
            else if (type == System::Byte::typeid)      { return (unsigned)ImpliedTyping::TypeCat::UInt8; }
            else if (type == System::SByte::typeid)     { return (unsigned)ImpliedTyping::TypeCat::Int8; }
            else if (type == System::UInt16::typeid)    { return (unsigned)ImpliedTyping::TypeCat::UInt16; }
            else if (type == System::Int16::typeid)     { return (unsigned)ImpliedTyping::TypeCat::Int16; }
            else if (type == System::UInt32::typeid)    { return (unsigned)ImpliedTyping::TypeCat::UInt32; }
            else if (type == System::Int32::typeid)     { return (unsigned)ImpliedTyping::TypeCat::Int32; }
            else if (type == System::Single::typeid)    { return (unsigned)ImpliedTyping::TypeCat::Float; }

            // else if (type == System::UInt64::typeid)    { return (unsigned)ImpliedTyping::TypeCat::UInt64; }
            // else if (type == System::Int64::typeid)     { return (unsigned)ImpliedTyping::TypeCat::Int64; }
            // else if (type == System::Double::typeid)    { return (unsigned)ImpliedTyping::TypeCat::Double; }
            else if (type == System::Char::typeid)      { return (unsigned)ImpliedTyping::TypeCat::UInt16; }

            return (unsigned)ImpliedTyping::TypeCat::Void;
        }

        static void SetupModifierKeys(RenderOverlays::DebuggingDisplay::InputSnapshot& evnt)
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

        static Tuple<Vector3, Vector3>^ CalculatePlacementCellBoundary(
            EditorSceneManager^ sceneMan,
            EntityInterface::DocumentId doc)
        {
            auto boundary = sceneMan->GetScene()._placementsEditor->CalculateCellBoundary(doc);
            return Tuple::Create(AsVector3(boundary.first), AsVector3(boundary.second));
        }

        static void GenerateUberSurfaceFromDEM(
            String^ uberSurfaceDirectory, String^ demFile,
            unsigned nodeDimensions, unsigned cellTreeDepth,
            IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::ConvertDEMData(
                clix::marshalString<clix::E_UTF8>(uberSurfaceDirectory).c_str(),
                clix::marshalString<clix::E_UTF8>(demFile).c_str(),
                nodeDimensions, cellTreeDepth, nativeProgress.get());
        }

        static void GenerateBlankUberSurface(
            String^ uberSurfaceDirectory, 
            unsigned newCellCountX, unsigned newCellCountY,
            unsigned nodeDimensions, unsigned cellTreeDepth,
            IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::GenerateBlankUberSurface(
                clix::marshalString<clix::E_UTF8>(uberSurfaceDirectory).c_str(),
                newCellCountX, newCellCountY,
                nodeDimensions, cellTreeDepth, nativeProgress.get());
        }

        static void GenerateStarterCells(
            TerrainConfig^ cfg,
            String^ uberSurfaceDirectory,
            IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::GenerateStarterCells(
                cfg->GetNative(),
                clix::marshalString<clix::E_UTF8>(uberSurfaceDirectory).c_str(),
                nativeProgress.get());
        }

        static unsigned DefaultResolutionForLayer(SceneEngine::TerrainCoverageId layerId)
        {
            switch (layerId) {
            case SceneEngine::CoverageId_AngleBasedShadows: return 1;
            default: return 4;
            }
        }

        static unsigned DefaultOverlapForLayer(SceneEngine::TerrainCoverageId layerId)
        {
            switch (layerId) {
            case SceneEngine::CoverageId_AngleBasedShadows: return 1;
            default: return 1;
            }
        }

        static unsigned DefaultFormatForLayer(SceneEngine::TerrainCoverageId layerId)
        {
            switch (layerId) {
            case SceneEngine::CoverageId_AngleBasedShadows: 
                return RenderCore::Metal::NativeFormat::R16G16_UNORM;
            default: return RenderCore::Metal::NativeFormat::R8_UINT;
            }
        }

        static TerrainConfig::CoverageLayerDesc^ DefaultCoverageLayer(
            TerrainConfig^ cfg, String^ baseDirectory, unsigned coverageLayerId) 
        {
            ::Assets::ResChar uberSurfaceFN[MaxPath]; 
            SceneEngine::TerrainConfig::GetUberSurfaceFilename(
                uberSurfaceFN, dimof(uberSurfaceFN),
                clix::marshalString<clix::E_UTF8>(baseDirectory).c_str(),
                coverageLayerId);

            auto result = gcnew TerrainConfig::CoverageLayerDesc(
                clix::marshalString<clix::E_UTF8>(uberSurfaceFN), coverageLayerId);
            
            const auto layerRes = DefaultResolutionForLayer(coverageLayerId);
            const auto overlap = DefaultOverlapForLayer(coverageLayerId);
            result->NodeDims = AsVectorUInt2(layerRes * AsUInt2(cfg->NodeDims));
            result->Overlap = overlap;
            result->Format = DefaultFormatForLayer(coverageLayerId);

            return result;
        }

        static void GenerateShadowsSurface(
            TerrainConfig^ cfg, String^ uberSurfaceDir,
            IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::GenerateShadowsSurface(
                cfg->GetNative(), clix::marshalString<clix::E_UTF8>(uberSurfaceDir).c_str(),
                true, nativeProgress.get());
        }

        static float GetSunPathAngle(EditorSceneManager^ sceneMan)
        {
            auto terr = sceneMan->GetScene()._terrainManager;
            if (!terr) return 0.f;

            return terr->GetConfig().SunPathAngle();
        }

        static void FlushTerrainToDisk(EditorSceneManager^ sceneMan, IProgress^ progress)
        {
            auto terr = sceneMan->GetScene()._terrainManager;
            if (!terr) return;

            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            terr->FlushToDisk(nativeProgress.get());
        }

        static void RebuildCellFiles(
            TerrainConfig^ cfg, String^ uberSurfaceDir, bool overwriteExisting, IProgress^ progress)
        {
            auto nativeProgress = progress ? IProgress::CreateNative(progress) : nullptr;
            ToolsRig::GenerateCellFiles(
                cfg->GetNative(), 
                clix::marshalString<clix::E_UTF8>(uberSurfaceDir).c_str(), 
                overwriteExisting, nativeProgress.get());
        }

    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    IEnumerable<String^>^ EnvironmentSettingsSet::Names::get()
    {
        auto result = gcnew List<String^>();
        for (auto i=_settings->cbegin(); i!=_settings->cend(); ++i)
            result->Add(clix::marshalString<clix::E_UTF8>(i->first));
        return result;
    }

    void EnvironmentSettingsSet::AddDefault()
    {
        _settings->push_back(
            std::make_pair(std::string("Default"), PlatformRig::DefaultEnvironmentSettings()));
    }

    const PlatformRig::EnvironmentSettings& EnvironmentSettingsSet::GetSettings(String^ name)
    {
        auto nativeName = clix::marshalString<clix::E_UTF8>(name);
        for (auto i=_settings->cbegin(); i!=_settings->cend(); ++i)
            if (!XlCompareStringI(nativeName.c_str(), i->first.c_str()))
                return i->second;
        if (!_settings->empty()) return (*_settings)[0].second;

        return *(const PlatformRig::EnvironmentSettings*)nullptr;
    }

    EnvironmentSettingsSet::EnvironmentSettingsSet(EditorSceneManager^ scene)
    {
        _settings.reset(new EnvSettingsVector());
        *_settings = BuildEnvironmentSettings(scene->GetFlexObjects());
    }

    EnvironmentSettingsSet::~EnvironmentSettingsSet() {}
}


