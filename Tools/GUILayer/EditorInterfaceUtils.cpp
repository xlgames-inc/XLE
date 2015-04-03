// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "GUILayerUtil.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/IntersectionTest.h"
#include "../../RenderCore/Techniques/Techniques.h"
#include "../../RenderCore/IDevice.h"

namespace GUILayer
{
    public ref class EditorInterfaceUtils
    {
    public:
        static clix::auto_ptr<SceneEngine::IntersectionTestContext>^
            CreateIntersectionTestContext(
                EngineDevice^ engineDevice,
                TechniqueContextWrapper^ techniqueContext)
        {
            std::shared_ptr<RenderCore::Techniques::TechniqueContext> nativeTC;
            if (techniqueContext) {
                nativeTC = *techniqueContext->_techniqueContext.get();
            } else {
                nativeTC = std::make_shared<RenderCore::Techniques::TechniqueContext>();
            }
            return gcnew clix::auto_ptr<SceneEngine::IntersectionTestContext>(
                new SceneEngine::IntersectionTestContext(
                    engineDevice->GetNative().GetRenderDevice()->GetImmediateContext(),
                    RenderCore::Techniques::CameraDesc(),
                    nativeTC));
        }

        // static System::Collections::Generic::ICollection<HitRecord^>^ 
        //     RayIntersection(
        //         EngineDevice^ engineDevice, 
        //         TechniqueContextWrapper^ techniqueContext,
        //         EditorSceneManager^ editorSceneManager,
        //         float worldSpaceRayStartX,
        //         float worldSpaceRayStartY,
        //         float worldSpaceRayStartZ,
        //         float worldSpaceRayEndX,
        //         float worldSpaceRayEndY,
        //         float worldSpaceRayEndZ,
        //         unsigned filter)
        // {
        //     std::shared_ptr<RenderCore::Techniques::TechniqueContext> nativeTC;
        //     if (techniqueContext) {
        //         nativeTC = *techniqueContext->_techniqueContext.get();
        //     } else {
        //         nativeTC = std::make_shared<RenderCore::Techniques::TechniqueContext>();
        //     }
        //     SceneEngine::IntersectionTestContext testContext(
        //         engineDevice->GetNative().GetRenderDevice()->GetImmediateContext(),
        //         RenderCore::Techniques::CameraDesc(),
        //         nativeTC);
        // 
        //     return editorSceneManager->RayIntersection(
        //         testContext, 
        //         Float3(worldSpaceRayStartX, worldSpaceRayStartY, worldSpaceRayStartZ),
        //         Float3(worldSpaceRayEndX, worldSpaceRayEndY, worldSpaceRayEndZ),
        //         ~0u);
        // }
    };
}


