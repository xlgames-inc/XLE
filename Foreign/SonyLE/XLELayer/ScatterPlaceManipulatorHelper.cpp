// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma warning(disable:4564)

#include "NativeManipulators.h"
#include "ManipulatorOverlay.h"
#include "XLELayerUtils.h"
#include "../../../Tools/GUILayer/ClixAutoPtr.h"
#include "../../../Tools/GUILayer/MarshalString.h"
#include "../../../Tools/ToolsRig/PlacementsManipulators.h"

using namespace System;
using namespace System::Drawing;
using namespace Sce::Atf::VectorMath;
using namespace System::Runtime::InteropServices;

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

        ref class Operation
        {
        public:
            List<Tuple<uint64, uint64>^>^ _toBeDeleted;
            List<Vec3F>^ _creationPositions;
        };

        Operation^ Perform(String^ modelName, Vec3F center, float radius, float density);

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
        Float3 intersection;
        if (GUILayer::EditorInterfaceUtils::GetTerrainUnderCursor(
                intersection[0], intersection[1], intersection[2], 
                hitTestContext, hitTestScene, pt.X, pt.Y)) {
            result = Vec3F(intersection[0], intersection[1], intersection[2]);
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
            center.X, center.Y, center.Z, radius);
    }

    ScatterPlaceManipulatorHelper::Operation^ ScatterPlaceManipulatorHelper::Perform(
        String^ modelName, Vec3F center, float radius, float density)
    {
            // Calculate the objects to be created and deleted in this operation
        std::vector<SceneEngine::PlacementGUID> toBeDeleted;
        std::vector<Float3> spawnPositions;

        // const float radius = 50.f;
        // const float density = .1f;
        auto sceneManager = NativeManipulatorLayer::SceneManager;

        auto editor = sceneManager->GetPlacementsEditor();
        auto scene = sceneManager->GetIntersectionScene();
        ToolsRig::CalculateScatterOperation(
            toBeDeleted, spawnPositions,
            editor->GetNative(), scene->GetNative(),
            clix::marshalString<clix::E_UTF8>(modelName).c_str(),
            AsFloat3(center), radius, density);

        delete editor;
        delete scene;

            // convert the returned objects into native types
        auto result = gcnew ScatterPlaceManipulatorHelper::Operation();
        result->_toBeDeleted = gcnew List<Tuple<uint64, uint64>^>();
        result->_creationPositions = gcnew List<Vec3F>();
        for (const auto& d:toBeDeleted) result->_toBeDeleted->Add(Tuple::Create(d.first, d.second & 0xffffffffull));
        for (const auto& p:spawnPositions) result->_creationPositions->Add(Vec3F(p[0], p[1], p[2]));

        return result;
    }

    ScatterPlaceManipulatorHelper::ScatterPlaceManipulatorHelper() {}
    ScatterPlaceManipulatorHelper::~ScatterPlaceManipulatorHelper() {}
}

