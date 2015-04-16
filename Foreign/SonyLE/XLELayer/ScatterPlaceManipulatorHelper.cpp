// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "NativeManipulators.h"
#include "ManipulatorOverlay.h"
#include "XLELayerUtils.h"
#include "../../../Core/Types.h"
#include <vector>
#include <utility>

using namespace System;
using namespace System::Drawing;
using namespace System::Runtime::InteropServices;
using namespace Sce::Atf::VectorMath;

namespace XLELayer
{
    public ref class ScatterPlaceManipulatorHelper
    {
    public:
         bool HitTest(
            [Out] Vec3F% result,
            Point pt, Drawing::Size viewportSize,
		    Sce::Atf::Rendering::Camera^ camera);
        void Render(LevelEditorCore::ViewControl^ vc, Vec3F center, float radius);

        GUILayer::EditorInterfaceUtils::ScatterPlaceOperation^ Perform(String^ modelName, Vec3F center, float radius, float density);

        ScatterPlaceManipulatorHelper();
        ~ScatterPlaceManipulatorHelper();
    };


    bool ScatterPlaceManipulatorHelper::HitTest(
        [Out] Vec3F% result,
        Point pt, Drawing::Size viewportSize,
		Sce::Atf::Rendering::Camera^ camera)
    {
        auto hitTestContext = GUILayer::EditorInterfaceUtils::CreateIntersectionTestContext(
			GUILayer::EngineDevice::GetInstance(), nullptr,
			XLELayerUtils::AsCameraDesc(camera),
            viewportSize.Width, viewportSize.Height);
        auto hitTestScene = NativeManipulatorLayer::SceneManager->GetIntersectionScene();

        bool gotPoint = false;
        GUILayer::Vector3 intersection;
        if (GUILayer::EditorInterfaceUtils::GetTerrainUnderCursor(
                intersection, 
                hitTestContext, hitTestScene, pt.X, pt.Y)) {
            result = AsVec3F(intersection);
            gotPoint = true;
        } else {
            result = Vec3F(0.f, 0.f, 0.f);
        }

        delete hitTestScene;
        delete hitTestContext;

        return gotPoint;
    }

    void ScatterPlaceManipulatorHelper::Render(
        LevelEditorCore::ViewControl^ vc, Vec3F center, float radius)
    {
        if (!ManipulatorOverlay::s_currentParsingContext) return;
        
        GUILayer::EditorInterfaceUtils::RenderCylinderHighlight(
            *ManipulatorOverlay::s_currentParsingContext,
            AsVector3(center), radius);
    }

    GUILayer::EditorInterfaceUtils::ScatterPlaceOperation^ ScatterPlaceManipulatorHelper::Perform(
        String^ modelName, Vec3F center, float radius, float density)
    {
            // Calculate the objects to be created and deleted in this operation
        auto sceneManager = NativeManipulatorLayer::SceneManager;

        auto editor = sceneManager->GetPlacementsEditor();
        auto scene = sceneManager->GetIntersectionScene();
        auto result = GUILayer::EditorInterfaceUtils::CalculateScatterOperation(
            editor, scene,
            modelName, AsVector3(center), radius, density);

        delete editor;
        delete scene;

        return result;
    }

    ScatterPlaceManipulatorHelper::ScatterPlaceManipulatorHelper() {}
    ScatterPlaceManipulatorHelper::~ScatterPlaceManipulatorHelper() {}
}

