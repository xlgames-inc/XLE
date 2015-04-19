// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "FlexGobInterface.h"
#include "../../Utility/StringUtils.h"

namespace GUILayer { namespace EditorDynamicInterface
{

///////////////////////////////////////////////////////////////////////////////////////////////////

	ObjectId FlexObjectType::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId typeId) const
    {
        return _nextObjectId++;
    }

	bool FlexObjectType::CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, const char initializer[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;

        for (auto i=_objects.cbegin(); i!=_objects.cend(); ++i)
            if (i->_doc == doc && i->_id == obj) return false;

        Object newObject;
        newObject._doc = doc;
        newObject._id = obj;
        newObject._type = typeId;
        newObject._parent = 0;
        _objects.push_back(std::move(newObject));

        InvokeOnChange(*type, _objects[_objects.size()-1]);
        return true;
    }

	bool FlexObjectType::DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const
    {
        for (auto i=_objects.cbegin(); i!=_objects.cend(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                assert(i->_type == objType);
                assert(i->_doc == doc);
                _objects.erase(i);
                return true;
            }
        return false;
    }

	bool FlexObjectType::SetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, 
        const void* src, unsigned elementType, unsigned arrayCount) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;
        if (!src) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                i->_properties.SetParameter(
                    propertyName.c_str(), src, 
                    ImpliedTyping::TypeDesc((ImpliedTyping::TypeCat)elementType, (uint16)arrayCount));

                InvokeOnChange(*type, *i);
                return true;
            }

        return false;
    }

	bool FlexObjectType::GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, void* dest, size_t* destSize) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                auto res = i->_properties.GetParameter<unsigned>(propertyName.c_str());
                if (res.first) {
                    *(unsigned*)dest = res.second;
                }
                return true;
            }

        return false;
    }

    bool FlexObjectType::SetParent(
        EditorScene& scene, DocumentId doc, 
        ObjectId child, ObjectTypeId childTypeId, 
        ObjectId parent, ObjectTypeId parentTypeId, int insertionPosition) const
    {
        auto childType = GetObjectType(childTypeId);
        if (!childType) return false;

        auto* childObj = GetObjectInt(doc, child);
        if (!childObj || childObj->_type != childTypeId)
            return false;

        if (childObj->_parent != 0) {
            auto* oldParent = GetObjectInt(doc, childObj->_parent);
            if (oldParent) {
                auto i = std::find(oldParent->_children.begin(), oldParent->_children.end(), child);
                oldParent->_children.erase(i);
            }
            childObj->_parent = 0;
        }

        if (!parent) return true;

        auto* parentObj = GetObjectInt(doc, parent);
        if (!parentObj || parentObj->_type != parentTypeId) return false;

        auto parentType = GetObjectType(parentTypeId);
        if (!parentType) return false;

        if (insertionPosition < 0 || insertionPosition >= (int)parentObj->_children.size()) {
            parentObj->_children.push_back(child);
        } else {
            parentObj->_children.insert(
                parentObj->_children.begin() + insertionPosition,
                child);
        }
        childObj->_parent = parentObj->_id;
        return true;
    }

	ObjectTypeId FlexObjectType::GetTypeId(const char name[]) const
    {
        for (auto i=_registeredObjectTypes.cbegin(); i!=_registeredObjectTypes.cend(); ++i)
            if (!XlCompareStringI(i->second._name.c_str(), name))
                return i->first;
        
        _registeredObjectTypes.push_back(
            std::make_pair(_nextObjectTypeId, RegisteredObjectType(name)));
        return _nextObjectTypeId++;
    }

	PropertyId FlexObjectType::GetPropertyId(ObjectTypeId typeId, const char name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_properties.cbegin(); i!=type->_properties.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)(1+std::distance(type->_properties.cbegin(), i));
        
        type->_properties.push_back(name);
        return (PropertyId)type->_properties.size();
    }

	ChildListId FlexObjectType::GetChildListId(ObjectTypeId typeId, const char name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_childLists.cbegin(); i!=type->_childLists.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)std::distance(type->_childLists.cbegin(), i);
        
        type->_childLists.push_back(name);
        return (PropertyId)(type->_childLists.size()-1);
    }

    auto FlexObjectType::GetObjectType(ObjectTypeId id) const -> RegisteredObjectType*
    {
        for (auto i=_registeredObjectTypes.begin(); i!=_registeredObjectTypes.end(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    bool FlexObjectType::RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange)
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        type->_onChange.push_back(std::move(onChange));
        return true;
    }

    void FlexObjectType::InvokeOnChange(RegisteredObjectType& type, Object& obj) const
    {
        for (auto i=type._onChange.begin(); i!=type._onChange.end(); ++i) {
            (*i)(*this, obj._doc, obj._id, obj._type);
        }

        if (obj._parent != 0)
            for (auto i=_objects.begin(); i!=_objects.end(); ++i)
                if (i->_id == obj._parent && i->_doc == obj._doc) {
                    auto type = GetObjectType(i->_type);
                    if (type) 
                        InvokeOnChange(*type, *i);
                }
    }

    auto FlexObjectType::GetObject(DocumentId doc, ObjectId obj) const -> const Object*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto FlexObjectType::GetObjectInt(DocumentId doc, ObjectId obj) const -> Object* 
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto FlexObjectType::FindObjectsOfType(ObjectTypeId typeId) const -> std::vector<const Object*>
    {
        std::vector<const Object*> result;
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_type == typeId) {
                result.push_back(AsPointer(i));
            }
        return std::move(result);
    }

    DocumentId FlexObjectType::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
    {
        return 0;
    }

	bool FlexObjectType::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
    {
        return false;
    }

	DocumentTypeId FlexObjectType::GetDocumentTypeId(const char name[]) const
    {
        return 0;
    }

	FlexObjectType::FlexObjectType()
    {
        _nextObjectTypeId = 1;
    }

	FlexObjectType::~FlexObjectType()
    {}


}}

