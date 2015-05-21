// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"

namespace SceneEngine
{
    class PlacementsManager;
    class PlacementsEditor;
}

namespace GUILayer { namespace EditorDynamicInterface
{
    class PlacementObjectType : public IObjectType
    {
    public:
        DocumentId CreateDocument(DocumentTypeId docType, const char initializer[]) const;
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType) const;

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type) const;
        bool CreateObject(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount) const;
        bool DeleteObject(const Identifier& id) const;
        bool SetProperty(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount) const;
        bool GetProperty(const Identifier& id, PropertyId prop, void* dest, unsigned* destSize) const;
        bool SetParent(const Identifier& child, const Identifier& parent, int insertionPosition) const;

        ObjectTypeId GetTypeId(const char name[]) const;
        DocumentTypeId GetDocumentTypeId(const char name[]) const;
        PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
        ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

        PlacementObjectType(
            std::shared_ptr<SceneEngine::PlacementsManager> manager,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlacementObjectType();

        static const DocumentTypeId DocumentType_Placements = 1;
        static const ObjectTypeId ObjectType_Placement = 1;
        static const PropertyId Property_Transform = 100;
        static const PropertyId Property_Visible = 101;
        static const PropertyId Property_Model = 102;
        static const PropertyId Property_Material = 103;
        static const PropertyId Property_Bounds = 104;
        static const PropertyId Property_LocalBounds = 105;

    protected:
        std::shared_ptr<SceneEngine::PlacementsManager> _manager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _editor;
    };
}}

