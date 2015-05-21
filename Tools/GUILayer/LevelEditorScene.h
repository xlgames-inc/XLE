// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "CLIXAutoPtr.h"
#include "../../Assets/Assets.h"    // just for ResChar
#include <memory>

using namespace System::Collections::Generic;

namespace SceneEngine 
{
    class PlacementsManager; class PlacementsEditor; 
    class TerrainManager;
    class ISceneParser; class IntersectionTestScene; 
}
namespace Tools { class IManipulator; }

namespace GUILayer
{
    ref class VisCameraSettings;
    ref class IntersectionTestContextWrapper;
	ref class IntersectionTestSceneWrapper;
    ref class PlacementsEditorWrapper;
    ref class ObjectSet;
    class TerrainGob;
    class ObjectPlaceholders;

    namespace EditorDynamicInterface { class RegisteredTypes; class FlexObjectScene; class FlexObjectType; }

    class EditorScene
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;
        std::shared_ptr<EditorDynamicInterface::FlexObjectScene> _flexObjects;
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

            //// //// ////   G O B   I N T E R F A C E   //// //// ////
        using DocumentTypeId = EditorDynamicInterface::DocumentTypeId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using DocumentId = EditorDynamicInterface::DocumentId;
        using ObjectId = EditorDynamicInterface::ObjectId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using PropertyId = EditorDynamicInterface::PropertyId;
        using ChildListId = EditorDynamicInterface::ChildListId;

        DocumentId CreateDocument(DocumentTypeId docType);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        value struct PropertyInitializer
        {
            PropertyId _prop;
            const void* _src;
            unsigned _elementType;
            unsigned _arrayCount;

            PropertyInitializer(PropertyId prop, const void* src, unsigned elementType, unsigned arrayCount)
                : _prop(prop), _src(src), _elementType(elementType), _arrayCount(arrayCount) {}
        };

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType, IEnumerable<PropertyInitializer>^ initializers);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, IEnumerable<PropertyInitializer>^ initializers);
        bool GetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, unsigned* destSize);

        bool SetObjectParent(DocumentId doc, 
            ObjectId childId, ObjectTypeId childTypeId, 
            ObjectId parentId, ObjectTypeId parentTypeId, int insertionPosition);

        ObjectTypeId GetTypeId(System::String^ name);
        DocumentTypeId GetDocumentTypeId(System::String^ name);
        PropertyId GetPropertyId(ObjectTypeId type, System::String^ name);
        ChildListId GetChildListId(ObjectTypeId type, System::String^ name);

        void SetTypeAnnotation(uint typeId, System::String^ annotationName, IEnumerable<PropertyInitializer>^ initializers);

            //// //// ////   E X P O R T   I N T E R F A C E   //// //// ////
        ref class ExportResult
        {
        public:
            System::String^ _messages;
            bool _success;
        };
        ExportResult^ ExportPlacements(DocumentId placementsDoc, System::String^ destinationFile);
        ExportResult^ ExportEnvironmentSettings(DocumentId docId, System::String^ destinationFile);
        ExportResult^ ExportTerrainSettings(System::String^ destinationFolder);

            //// //// ////   U T I L I T Y   //// //// ////
        const EditorDynamicInterface::FlexObjectScene& GetFlexObjects();
        void IncrementTime(float increment);

            //// //// ////   C O N S T R U C T O R S   //// //// ////
        EditorSceneManager();
        ~EditorSceneManager();
        !EditorSceneManager();
    protected:
        clix::shared_ptr<EditorScene> _scene;
        clix::shared_ptr<EditorDynamicInterface::RegisteredTypes> _dynInterface;
        clix::shared_ptr<EditorDynamicInterface::FlexObjectType> _flexGobInterface;
    };
}


