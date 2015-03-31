// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "AutoToShared.h"
#include "../../Math/Vector.h"
#include <memory>

namespace SceneEngine { class PlacementsManager; class PlacementsEditor; class ISceneParser; class IntersectionTestContext; }

namespace GUILayer
{
    ref class VisCameraSettings;

    class EditorScene
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;

        EditorScene();
    };

    namespace EditorDynamicInterface { class RegisteredTypes; }

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

    ref class IOverlaySystem;

    public ref class EditorSceneManager
    {
    public:
        using DocumentTypeId = EditorDynamicInterface::DocumentTypeId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using DocumentId = EditorDynamicInterface::DocumentId;
        using ObjectId = EditorDynamicInterface::ObjectId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using PropertyId = EditorDynamicInterface::PropertyId;
        using ChildListId = EditorDynamicInterface::ChildListId;

        DocumentId CreateDocument(DocumentTypeId docType, System::String^ initializer);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType, System::String^ initializer);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* data);

        ObjectTypeId GetTypeId(System::String^ name);
        DocumentTypeId GetDocumentTypeId(System::String^ name);
        PropertyId GetPropertyId(ObjectTypeId type, System::String^ name);
        ChildListId GetChildListId(ObjectTypeId type, System::String^ name);

        IOverlaySystem^ CreateOverlaySystem(VisCameraSettings^ camera);

        System::Collections::Generic::IEnumerable<HitRecord^>^ 
            RayIntersection(
                const SceneEngine::IntersectionTestContext& testContext,
                Float3 worldSpaceRayStart, Float3 worldSpaceRayEnd);

        EditorSceneManager();
        ~EditorSceneManager();
        !EditorSceneManager();
    protected:
        AutoToShared<EditorScene> _scene;
        AutoToShared<EditorDynamicInterface::RegisteredTypes> _dynInterface;
    };
}
