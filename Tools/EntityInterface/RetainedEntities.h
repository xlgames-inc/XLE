// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EntityInterface.h"
#include "../../Utility/ParameterBox.h"
#include "../../Assets/Assets.h"        // for rstring
#include <string>
#include <vector>
#include <functional>

namespace EntityInterface
{
    class RetainedEntity
    {
    public:
        ObjectId _id;
        DocumentId _doc;
        ObjectTypeId _type;

        ParameterBox _properties;
        std::vector<ObjectId> _children;
        ObjectId _parent;
    };

    /// <summary>Stores entity data generically</summary>
    /// This implemention simply stores all information that comes from IObjectType
    /// in a generic data structure.
    ///
    /// Clients can put callbacks on specific object types to watch for changes.
    /// This can make it easier to implement lightweight object types. Instead of
    /// having to implement the IEntityInterface, simply set a callback with
    /// RegisterCallback().
    ///
    /// All of the properties and data related to that object will be available in
    /// the callback.
    class RetainedEntities
    {
    public:
        const RetainedEntity* GetEntity(DocumentId doc, ObjectId obj) const;
        const RetainedEntity* GetEntity(const Identifier&) const;
        std::vector<const RetainedEntity*> FindEntitiesOfType(ObjectTypeId typeId) const;

        using OnChangeDelegate = 
            std::function<
                void(const RetainedEntities& flexSys, const Identifier&)
            >;
        bool RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange);

        ObjectTypeId    GetTypeId(const char name[]) const;
		PropertyId      GetPropertyId(ObjectTypeId typeId, const char name[]) const;
		ChildListId     GetChildListId(ObjectTypeId typeId, const char name[]) const;

        RetainedEntities();
        ~RetainedEntities();
    protected:
        mutable ObjectId _nextObjectId;
        mutable std::vector<RetainedEntity> _objects;

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
        void InvokeOnChange(RegisteredObjectType& type, RetainedEntity& obj) const;
        RetainedEntity* GetEntityInt(DocumentId doc, ObjectId obj) const;
        bool SetSingleProperties(RetainedEntity& dest, const RegisteredObjectType& type, const PropertyInitializer& initializer) const;

        friend class RetainedEntityInterface;
    };

    /// <summary>Implements IEntityInterface for retained entities</summary>
    /// This implementation will simply accept all incoming data, and store
    /// it in a generic data structure.
    class RetainedEntityInterface : public IEntityInterface
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

		ObjectTypeId    GetTypeId(const char name[]) const;
		DocumentTypeId  GetDocumentTypeId(const char name[]) const;
		PropertyId      GetPropertyId(ObjectTypeId typeId, const char name[]) const;
		ChildListId     GetChildListId(ObjectTypeId typeId, const char name[]) const;

		RetainedEntityInterface(std::shared_ptr<RetainedEntities> scene);
		~RetainedEntityInterface();
    protected:
        std::shared_ptr<RetainedEntities> _scene;
    };
}



