// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EntityLayer.h"
#include "CLIXAutoPtr.h"
#include "../EntityInterface/EntityInterface.h"
#include <memory>

using namespace System::Collections::Generic;

namespace SceneEngine 
{
    class PlacementsManager; class PlacementsEditor; 
    class TerrainManager;
    class ISceneParser; class IntersectionTestScene; 
}
namespace Tools { class IManipulator; }

namespace EntityInterface { class Switch; class RetainedEntities; class RetainedEntityInterface; }

namespace GUILayer
{
    ref class VisCameraSettings;
    ref class IntersectionTestContextWrapper;
	ref class IntersectionTestSceneWrapper;
    ref class PlacementsEditorWrapper;
    ref class ObjectSet;
    class TerrainGob;
    class ObjectPlaceholders;

    class EditorScene
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;
        std::shared_ptr<EntityInterface::RetainedEntities> _flexObjects;
        std::shared_ptr<ObjectPlaceholders> _placeholders;

        void    IncrementTime(float increment) { _currentTime += increment; }
        float   _currentTime;

        EditorScene();
		~EditorScene();
    };

    public ref class EditorSceneRenderSettings
    {
    public:
        System::String^ _activeEnvironmentSettings;
        ObjectSet^ _selection;
    };

    ref class IOverlaySystem;
    ref class IManipulatorSet;
    ref class IPlacementManipulatorSettingsLayer;

    public ref class EditorSceneManager
    {
    public:
            //// //// ////   U T I L S   //// //// ////
        IManipulatorSet^ CreateTerrainManipulators();
        IManipulatorSet^ CreatePlacementManipulators(IPlacementManipulatorSettingsLayer^ context);
        IOverlaySystem^ CreateOverlaySystem(VisCameraSettings^ camera, EditorSceneRenderSettings^ renderSettings);
		IntersectionTestSceneWrapper^ GetIntersectionScene();
        PlacementsEditorWrapper^ GetPlacementsEditor();
        EntityLayer^ GetEntityInterface();

        void SetTypeAnnotation(
            uint typeId, System::String^ annotationName, 
            IEnumerable<EntityLayer::PropertyInitializer>^ initializers);

            //// //// ////   E X P O R T   I N T E R F A C E   //// //// ////
        ref class ExportResult
        {
        public:
            System::String^ _messages;
            bool _success;
        };
        ExportResult^ ExportPlacements(EntityInterface::DocumentId placementsDoc, System::String^ destinationFile);
        ExportResult^ ExportEnvironmentSettings(EntityInterface::DocumentId docId, System::String^ destinationFile);
        ExportResult^ ExportTerrainSettings(System::String^ destinationFolder);

            //// //// ////   U T I L I T Y   //// //// ////
        const EntityInterface::RetainedEntities& GetFlexObjects();
        void IncrementTime(float increment);

            //// //// ////   C O N S T R U C T O R S   //// //// ////
        EditorSceneManager();
        ~EditorSceneManager();
        !EditorSceneManager();
    protected:
        clix::shared_ptr<EditorScene> _scene;
        clix::shared_ptr<::EntityInterface::RetainedEntityInterface> _flexGobInterface;
        EntityLayer^ _entities;
    };
}


