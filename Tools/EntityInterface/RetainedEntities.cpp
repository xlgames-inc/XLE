// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "RetainedEntities.h"
#include "../../Utility/StringUtils.h"

namespace EntityInterface
{
    bool RetainedEntities::SetSingleProperties(
        RetainedEntity& dest, const RegisteredObjectType& type, const PropertyInitializer& prop) const
    {
        if (prop._prop == 0 || prop._prop > type._properties.size()) return false;
        if (!prop._src) return false;

        const auto& propertyName = type._properties[prop._prop-1];
        dest._properties.SetParameter(
            propertyName.c_str(), prop._src, 
            ImpliedTyping::TypeDesc((ImpliedTyping::TypeCat)prop._elementType, (uint16)prop._arrayCount));
        return true;
    }

    auto RetainedEntities::GetObjectType(ObjectTypeId id) const -> RegisteredObjectType*
    {
        for (auto i=_registeredObjectTypes.begin(); i!=_registeredObjectTypes.end(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    bool RetainedEntities::RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange)
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        type->_onChange.push_back(std::move(onChange));
        return true;
    }

    void RetainedEntities::InvokeOnChange(RegisteredObjectType& type, RetainedEntity& obj) const
    {
        for (auto i=type._onChange.begin(); i!=type._onChange.end(); ++i) {
            (*i)(*this, Identifier(obj._doc, obj._id, obj._type));
        }

        if (obj._parent != 0)
            for (auto i=_objects.begin(); i!=_objects.end(); ++i)
                if (i->_id == obj._parent && i->_doc == obj._doc) {
                    auto type = GetObjectType(i->_type);
                    if (type) 
                        InvokeOnChange(*type, *i);
                }
    }

    auto RetainedEntities::GetEntity(DocumentId doc, ObjectId obj) const -> const RetainedEntity*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto RetainedEntities::GetEntity(const Identifier& id) const -> const RetainedEntity*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == id.Object() && i->_doc == id.Document() && i->_type == id.ObjectType())
                return AsPointer(i);
        return nullptr;
    }

    auto RetainedEntities::GetEntityInt(DocumentId doc, ObjectId obj) const -> RetainedEntity* 
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto RetainedEntities::FindEntitiesOfType(ObjectTypeId typeId) const -> std::vector<const RetainedEntity*>
    {
        std::vector<const RetainedEntity*> result;
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_type == typeId) {
                result.push_back(AsPointer(i));
            }
        return std::move(result);
    }

    ObjectTypeId RetainedEntities::GetTypeId(const utf8 name[]) const
    {
        for (auto i=_registeredObjectTypes.cbegin(); i!=_registeredObjectTypes.cend(); ++i)
            if (!XlCompareStringI(i->second._name.c_str(), name))
                return i->first;
        
        _registeredObjectTypes.push_back(
            std::make_pair(_nextObjectTypeId, RegisteredObjectType(name)));
        return _nextObjectTypeId++;
    }

	PropertyId RetainedEntities::GetPropertyId(ObjectTypeId typeId, const utf8 name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_properties.cbegin(); i!=type->_properties.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)(1+std::distance(type->_properties.cbegin(), i));
        
        type->_properties.push_back(name);
        return (PropertyId)type->_properties.size();
    }

	ChildListId RetainedEntities::GetChildListId(ObjectTypeId typeId, const utf8 name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_childLists.cbegin(); i!=type->_childLists.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)std::distance(type->_childLists.cbegin(), i);
        
        type->_childLists.push_back(name);
        return (PropertyId)(type->_childLists.size()-1);
    }

    std::basic_string<utf8> RetainedEntities::GetTypeName(ObjectTypeId id) const
    {
        auto i = LowerBound(_registeredObjectTypes, id);
        if (i != _registeredObjectTypes.end() && i->first == id) {
            return i->second._name;
        }
        return std::basic_string<utf8>();
    }

    RetainedEntities::RetainedEntities()
    {
        _nextObjectTypeId = 1;
        _nextObjectId = 1;
    }

    RetainedEntities::~RetainedEntities() {}

///////////////////////////////////////////////////////////////////////////////////////////////////

	ObjectId RetainedEntityInterface::AssignObjectId(DocumentId doc, ObjectTypeId typeId) const
    {
        return _scene->_nextObjectId++;
    }

	bool RetainedEntityInterface::CreateObject(const Identifier& id, 
        const PropertyInitializer initializers[], size_t initializerCount)
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;

        for (auto i=_scene->_objects.cbegin(); i!=_scene->_objects.cend(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) return false;

        RetainedEntity newObject;
        newObject._doc = id.Document();
        newObject._id = id.Object();
        newObject._type = id.ObjectType();
        newObject._parent = 0;

        for (size_t c=0; c<initializerCount; ++c)
            _scene->SetSingleProperties(newObject, *type, initializers[c]);

        _scene->_objects.push_back(std::move(newObject));

        _scene->InvokeOnChange(*type, _scene->_objects[_scene->_objects.size()-1]);
        return true;
    }

	bool RetainedEntityInterface::DeleteObject(const Identifier& id)
    {
        for (auto i=_scene->_objects.cbegin(); i!=_scene->_objects.cend(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                assert(i->_type == id.ObjectType());
                _scene->_objects.erase(i);
                return true;
            }
        return false;
    }

	bool RetainedEntityInterface::SetProperty(
        const Identifier& id, 
        const PropertyInitializer initializers[], size_t initializerCount)
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;

        for (auto i=_scene->_objects.begin(); i!=_scene->_objects.end(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                bool gotChange = false;
                for (size_t c=0; c<initializerCount; ++c) {
                    auto& prop = initializers[c];
                    gotChange |= _scene->SetSingleProperties(*i, *type, prop);
                }
                if (gotChange) _scene->InvokeOnChange(*type, *i);
                return true;
            }

        return false;
    }

	bool RetainedEntityInterface::GetProperty(const Identifier& id, PropertyId prop, void* dest, unsigned* destSize) const
    {
        auto type = _scene->GetObjectType(id.ObjectType());
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_scene->_objects.begin(); i!=_scene->_objects.end(); ++i)
            if (i->_doc == id.Document() && i->_id == id.Object()) {
                auto res = i->_properties.GetParameter<unsigned>(propertyName.c_str());
                if (res.first) {
                    *(unsigned*)dest = res.second;
                }
                return true;
            }

        return false;
    }

    bool RetainedEntityInterface::SetParent(
        const Identifier& child, const Identifier& parent, int insertionPosition)
    {
        if (child.Document() != parent.Document())
            return false;

        auto childType = _scene->GetObjectType(child.ObjectType());
        if (!childType) return false;

        auto* childObj = _scene->GetEntityInt(child.Document(), child.Object());
        if (!childObj || childObj->_type != child.ObjectType())
            return false;

        if (childObj->_parent != 0) {
            auto* oldParent = _scene->GetEntityInt(child.Document(), childObj->_parent);
            if (oldParent) {
                auto i = std::find(oldParent->_children.begin(), oldParent->_children.end(), child.Object());
                oldParent->_children.erase(i);
            }
            childObj->_parent = 0;
        }

        _scene->InvokeOnChange(*childType, *childObj);

            // if parent is set to 0, then this is a "remove from parent" operation
        if (!parent.Object()) return true;

        auto* parentObj = _scene->GetEntityInt(parent.Document(), parent.Object());
        if (!parentObj || parentObj->_type != parent.ObjectType()) return false;

        auto parentType = _scene->GetObjectType(parent.ObjectType());
        if (!parentType) return false;

        if (insertionPosition < 0 || insertionPosition >= (int)parentObj->_children.size()) {
            parentObj->_children.push_back(child.Object());
        } else {
            parentObj->_children.insert(
                parentObj->_children.begin() + insertionPosition,
                child.Object());
        }
        childObj->_parent = parentObj->_id;

        _scene->InvokeOnChange(*parentType, *parentObj);
        return true;
    }

	ObjectTypeId    RetainedEntityInterface::GetTypeId(const char name[]) const
    {
        return _scene->GetTypeId((const utf8*)name);
    }

	PropertyId      RetainedEntityInterface::GetPropertyId(ObjectTypeId typeId, const char name[]) const
    {
        return _scene->GetPropertyId(typeId, (const utf8*)name);
    }

	ChildListId     RetainedEntityInterface::GetChildListId(ObjectTypeId typeId, const char name[]) const
    {
        return _scene->GetPropertyId(typeId, (const utf8*)name);
    }

    DocumentId RetainedEntityInterface::CreateDocument(DocumentTypeId docType, const char initializer[])
    {
        return 0;
    }

	bool RetainedEntityInterface::DeleteDocument(DocumentId doc, DocumentTypeId docType)
    {
        return false;
    }

    DocumentTypeId RetainedEntityInterface::GetDocumentTypeId(const char name[]) const
    {
        return 0;
    }

	RetainedEntityInterface::RetainedEntityInterface(std::shared_ptr<RetainedEntities> flexObjects)
    : _scene(std::move(flexObjects))
    {}

	RetainedEntityInterface::~RetainedEntityInterface()
    {}

}

