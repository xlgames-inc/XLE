// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EditorInterfaceUtils.h"
#include "EditorDynamicInterface.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../SceneEngine/Terrain.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/IDevice.h"
#include "../../Assets/Assets.h"
#include "../ToolsRig/ManipulatorsUtil.h"
// #include "../../ConsoleRig/Log.h"        (can't include in Win32 managed code)

using namespace System::Runtime::InteropServices;

namespace GUILayer
{
	public ref class HitRecord
	{
	public:
		EditorDynamicInterface::DocumentId _document;
		EditorDynamicInterface::ObjectId _object;
		float _distance;
		float _worldSpaceCollisionX;
		float _worldSpaceCollisionY;
		float _worldSpaceCollisionZ;
	};

    void ObjectSet::Add(uint64 document, uint64 id)
    {
        _nativePlacements->push_back(std::make_pair(document, id));
    }

	ObjectSet::ObjectSet()     { _nativePlacements.reset(new NativePlacementSet); }
	ObjectSet::~ObjectSet()    { _nativePlacements.reset(); }

    public ref class EditorInterfaceUtils
    {
    public:
        static IntersectionTestContextWrapper^
            CreateIntersectionTestContext(
                EngineDevice^ engineDevice,
                TechniqueContextWrapper^ techniqueContext,
                const RenderCore::Techniques::CameraDesc& camera,
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
                    camera,
                    std::make_shared<RenderCore::ViewportContext>(UInt2(viewportWidth, viewportHeight)),
                    nativeTC));
        }

		static System::Collections::Generic::ICollection<HitRecord^>^
			RayIntersection(
				IntersectionTestSceneWrapper^ testScene,
				IntersectionTestContextWrapper^ testContext,
				float startX, float startY, float startZ,
				float endX, float endY, float endZ,
				unsigned filter)
		{
			TRY
			{
				auto firstResult = testScene->_scene->FirstRayIntersection(
					*testContext->_context.get(),
					std::make_pair(Float3(startX, startY, startZ), Float3(endX, endY, endZ)),
					filter);

				if (firstResult._type != 0) {
					auto record = gcnew HitRecord;
					record->_document = firstResult._objectGuid.first;
					record->_object = firstResult._objectGuid.second;
					record->_distance = firstResult._distance;
					record->_worldSpaceCollisionX = firstResult._worldSpaceCollision[0];
					record->_worldSpaceCollisionY = firstResult._worldSpaceCollision[1];
					record->_worldSpaceCollisionZ = firstResult._worldSpaceCollision[2];

					// hack -- for placement objects, we must strip off the top 32 bits
					//          from the object id.
					if (firstResult._type == SceneEngine::IntersectionTestScene::Type::Placement) {
						record->_object &= 0x00000000ffffffffull;
					}

					auto result = gcnew System::Collections::Generic::List<HitRecord^>();
					result->Add(record);
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

        static System::Collections::Generic::ICollection<HitRecord^>^
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

                    auto result = gcnew System::Collections::Generic::List<HitRecord^>();
                    for (auto i: nativeResults) {
					    auto record = gcnew HitRecord;
					    record->_document = i._objectGuid.first;
					    record->_object = i._objectGuid.second;
					    record->_distance = i._distance;
					    record->_worldSpaceCollisionX = i._worldSpaceCollision[0];
					    record->_worldSpaceCollisionY = i._worldSpaceCollision[1];
					    record->_worldSpaceCollisionZ = i._worldSpaceCollision[2];

					        // hack -- for placement objects, we must strip off the top 32 bits
					        //          from the object id.
					    if (i._type == SceneEngine::IntersectionTestScene::Type::Placement) {
						    record->_object &= 0x00000000ffffffffull;
					    }
					
					    result->Add(record);
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
            float% result,
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
            [Out] float% intersectionX,
            [Out] float% intersectionY,
            [Out] float% intersectionZ,
            IntersectionTestContextWrapper^ context,
            IntersectionTestSceneWrapper^ testScene,
            unsigned ptx, unsigned pty)
        {
            auto test = testScene->_scene->UnderCursor(
                *context->_context.get(), UInt2(ptx, pty),
                SceneEngine::IntersectionTestScene::Type::Terrain);
            if (test._type == SceneEngine::IntersectionTestScene::Type::Terrain) {
                intersectionX = test._worldSpaceCollision[0];
                intersectionY = test._worldSpaceCollision[1];
                intersectionZ = test._worldSpaceCollision[2];
                return true;
            }
            return false;
        }

        static void RenderCylinderHighlight(
            SceneEngine::LightingParserContext& parsingContext, 
            float centreX, float centreY, float centreZ, float radius)
        {
            ToolsRig::RenderCylinderHighlight(
                EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext().get(),
                parsingContext, Float3(centreX, centreY, centreZ), radius);
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
    };
}


