// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "../../Utility/ParameterBox.h"
#include "../../Assets/Assets.h"        // for rstring
#include <string>
#include <vector>
#include <functional>

namespace GUILayer { namespace EditorDynamicInterface
{
    class FlexObjectType : public IObjectType
    {
    public:
        DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
		bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

		ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, const PropertyInitializer initializers[], size_t initializerCount) const;
		bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
		bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, const PropertyInitializer initializers[], size_t initializerCount) const;
		bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, void* dest, unsigned* destSize) const;
        bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const;

		ObjectTypeId GetTypeId(const char name[]) const;
		DocumentTypeId GetDocumentTypeId(const char name[]) const;
		PropertyId GetPropertyId(ObjectTypeId typeId, const char name[]) const;
		ChildListId GetChildListId(ObjectTypeId typeId, const char name[]) const;

        using OnChangeDelegate = std::function<void(const FlexObjectType& flexSys, DocumentId, ObjectId, ObjectTypeId)>;
        bool RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange);

        class Object
        {
        public:
            ObjectId _id;
            DocumentId _doc;
            ObjectTypeId _type;

            ParameterBox _properties;
            std::vector<ObjectId> _children;
            ObjectId _parent;
        };
        const Object* GetObject(DocumentId doc, ObjectId obj) const;

        std::vector<const Object*> FindObjectsOfType(ObjectTypeId typeId) const;

		FlexObjectType();
		~FlexObjectType();

    protected:
        mutable ObjectId _nextObjectId;
        mutable std::vector<Object> _objects;

        class RegisteredObjectType
        {
        public:
            std::string _name;
            std::vector<std::string> _properties;
            std::vector<std::string> _childLists;

            std::vector<OnChangeDelegate> _onChange;

            RegisteredObjectType(const std::string& name) : _name(name) {}
        };
        mutable std::vector<std::pair<ObjectTypeId, RegisteredObjectType>> _registeredObjectTypes;

        mutable ObjectTypeId _nextObjectTypeId;

        RegisteredObjectType* GetObjectType(ObjectTypeId id) const;
        void InvokeOnChange(RegisteredObjectType& type, Object& obj) const;

        Object* GetObjectInt(DocumentId doc, ObjectId obj) const;

        bool SetSingleProperties(Object& dest, const RegisteredObjectType& type, const PropertyInitializer& initializer) const;
    };
}}



