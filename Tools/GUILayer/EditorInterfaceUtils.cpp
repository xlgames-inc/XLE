// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EditorDynamicInterface.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/IDevice.h"
#include "../../Assets/Assets.h"
#include "../../ConsoleRig/Log.h"

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

    public ref class EditorInterfaceUtils
    {
    public:
        static IntersectionTestContextWrapper^
            CreateIntersectionTestContext(
                EngineDevice^ engineDevice,
                TechniqueContextWrapper^ techniqueContext,
                const RenderCore::Techniques::CameraDesc& camera)
        {
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> nativeTC;
            if (techniqueContext) {
                nativeTC = *techniqueContext->_techniqueContext.get();
            } else {
                nativeTC = std::make_shared<RenderCore::Techniques::TechniqueContext>();
            }
            return gcnew IntersectionTestContextWrapper(
                std::make_shared<SceneEngine::IntersectionTestContext>(
                    engineDevice->GetNative().GetRenderDevice()->GetImmediateContext(),
                    camera,
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
				auto firstResult = (*testScene->_scene)->FirstRayIntersection(
					**testContext->_context,
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
				LogWarning << "Invalid resource while performing ray intersection test: {" << e.what() << "}";
			}
			CATCH(const ::Assets::Exceptions::PendingResource&) {}
			CATCH_END

			return nullptr;
		}
    };
}


