// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "XLELayerUtils.h"
#include "../../../Core/Types.h"
#include <msclr\auto_handle.h>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace Sce::Atf::VectorMath;

namespace XLEBridgeUtils
{
    public ref class Picking
    {
    public:
        [Flags]
        enum struct Flags : unsigned
        {
            Terrain = 1 << 0, Objects = 1 << 1, Helpers = 1 << 6, 
            AllWorldObjects = Terrain | Objects,
            IgnoreSelection = 1 << 10 
        };

        [StructLayout(LayoutKind::Sequential)]
        value struct HitRecord
        {
            uint64 documentId;
            uint64 instanceId;    // instance id of the game object.
            uint index;          // index of sub-object (if any )
            float distance;      // distance from camera ( for sorting )
            Vec3F hitPt;       // hit point in world space.
            Vec3F normal;      // world normal at hit-point.       
            Vec3F nearestVertex; //  nearest vertex in world space.
            bool hasNormal;
            bool hasNearestVert;
            short pad;

            uint32 drawCallIndex;
            uint64 materialGuid;
            String^ materialName;
            String^ modelName;
        };

        static array<HitRecord>^ FrustumPick(
            GUILayer::EngineDevice^ device,
            GUILayer::EditorSceneManager^ sceneManager,
            GUILayer::TechniqueContextWrapper^ techniqueContext,
            Matrix4F^ pickingFrustum, 
            Sce::Atf::Rendering::Camera^ camera,
            System::Drawing::Size viewportSize,
            Flags flags)
        {
            System::Diagnostics::Debug::Assert(unsigned(flags & Flags::IgnoreSelection) == 0);

            ICollection<GUILayer::HitRecord>^ results = nullptr;
            {
                msclr::auto_handle<GUILayer::IntersectionTestContextWrapper> context(
                    Utils::CreateIntersectionTestContext(
                        device, techniqueContext, camera, 
                        (unsigned)viewportSize.Width, (unsigned)viewportSize.Height));

                msclr::auto_handle<GUILayer::IntersectionTestSceneWrapper> scene(
                    sceneManager->GetIntersectionScene());

                cli::pin_ptr<float> ptr = &pickingFrustum->M11;
                results = GUILayer::EditorInterfaceUtils::FrustumIntersection(
                    scene.get(), context.get(), ptr, (uint)flags);
            }

            if (results == nullptr) { return nullptr; }
            return AsHitRecordArray(results);
        }

        static array<HitRecord>^ AsHitRecordArray(ICollection<GUILayer::HitRecord>^ results)
        {
            auto hitRecords = gcnew array<HitRecord>(results->Count);
            uint index = 0;
            for each(auto r in results)
            {
                hitRecords[index].documentId = r._document;
                hitRecords[index].instanceId = r._object;
                hitRecords[index].index = 0;
                hitRecords[index].distance = r._distance;
                hitRecords[index].hitPt = Utils::AsVec3F(r._worldSpaceCollision);
                hitRecords[index].normal = Vec3F(0.0f, 0.0f, 0.0f);
                hitRecords[index].nearestVertex = Vec3F(0.0f, 0.0f, 0.0f);
                hitRecords[index].hasNormal = hitRecords[index].hasNearestVert = false;
                hitRecords[index].drawCallIndex = r._drawCallIndex;
                hitRecords[index].materialGuid = r._materialGuid;
                hitRecords[index].materialName = r._materialName;
                hitRecords[index].modelName = r._modelName;
                index++;
            }

            return hitRecords;
        }

        static array<HitRecord>^ RayPick(
            GUILayer::EngineDevice^ device,
            GUILayer::EditorSceneManager^ sceneManager,
            GUILayer::TechniqueContextWrapper^ techniqueContext,
            Ray3F ray, Sce::Atf::Rendering::Camera^ camera, 
            System::Drawing::Size viewportSize, Flags flags)
        {
            System::Diagnostics::Debug::Assert(unsigned(flags & Flags::IgnoreSelection) == 0);

            ICollection<GUILayer::HitRecord>^ results = nullptr; 
            auto endPt = ray.Origin + camera->FarZ * ray.Direction;

            {
                msclr::auto_handle<GUILayer::IntersectionTestContextWrapper> context(
                    Utils::CreateIntersectionTestContext(
                        device, techniqueContext, camera, 
                        (uint)viewportSize.Width, (uint)viewportSize.Height));

                msclr::auto_handle<GUILayer::IntersectionTestSceneWrapper> scene(
                    sceneManager->GetIntersectionScene());

                results = GUILayer::EditorInterfaceUtils::RayIntersection(
                    scene.get(), context.get(),
                    Utils::AsVector3(ray.Origin),
                    Utils::AsVector3(endPt), (uint)flags);
            }

            if (results == nullptr) { return nullptr; }
            return AsHitRecordArray(results);
        }

        static array<HitRecord>^ RayPick(
            LevelEditorCore::ViewControl^ vc,
            Ray3F ray, Flags flags)
        {
            auto sceneMan = Utils::GetSceneManager(vc);
            if (!sceneMan) return nullptr;
            
            return RayPick(
                GUILayer::EngineDevice::GetInstance(),
                sceneMan,
                Utils::GetTechniqueContext(vc),
                ray, vc->Camera, vc->Size, flags);
        }

        static array<HitRecord>^ FrustumPick(
            LevelEditorCore::ViewControl^ vc,
            Matrix4F^ pickingFrustum, Flags flags)
        {
            auto sceneMan = Utils::GetSceneManager(vc);
            if (!sceneMan) return nullptr;
            
            return FrustumPick(
                GUILayer::EngineDevice::GetInstance(),
                sceneMan,
                Utils::GetTechniqueContext(vc),
                pickingFrustum, vc->Camera, vc->Size, flags);
        }
    };
}

