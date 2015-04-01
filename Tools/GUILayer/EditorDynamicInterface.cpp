// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EditorDynamicInterface.h"
#include "../../Utility/StringUtils.h"

namespace GUILayer { namespace EditorDynamicInterface
{
    IObjectType::~IObjectType() {}

    DocumentId RegisteredTypes::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->CreateDocument(scene, reg._mappedTypeId, initializer);
        }
        return 0;
    }

    bool RegisteredTypes::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->DeleteDocument(scene, doc, reg._mappedTypeId);
        }
        return false;
    }
            
    ObjectId RegisteredTypes::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId objType) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->AssignObjectId(scene, doc, reg._mappedTypeId);
        }
        return 0;
    }

    bool RegisteredTypes::CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const char initializer[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->CreateObject(scene, doc, obj, reg._mappedTypeId, initializer);
        }
        return false;
    }

    bool RegisteredTypes::DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->DeleteObject(scene, doc, obj, reg._mappedTypeId);
        }
        return false;
    }

    bool RegisteredTypes::SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const void* src, size_t srcSize) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->SetProperty(scene, doc, obj, reg._mappedTypeId, prop, src, srcSize);
        }
        return false;
    }

    bool RegisteredTypes::GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, size_t* destSize) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetProperty(scene, doc, obj, reg._mappedTypeId, prop, dest, destSize);
        }
        return false;
    }

    ObjectTypeId RegisteredTypes::GetTypeId(const char name[]) const
    {
        for (auto i=_knownObjectTypes.begin(); i!=_knownObjectTypes.end(); ++i) {
            if (!XlCompareString(i->_name.c_str(), name)) {
                return 1+(ObjectTypeId)std::distance(_knownObjectTypes.begin(), i);
            }
        }
        for (auto i=_types.cbegin(); i!=_types.cend(); ++i) {
            auto id = (*i)->GetTypeId(name);
            if (id != 0) {
                KnownType t;
                t._owner = *i;
                t._mappedTypeId = id;
                t._name = name;
                _knownObjectTypes.push_back(std::move(t));
                return 1+(ObjectTypeId)(_knownObjectTypes.size()-1);
            }
        }
        return 0;
    }

    DocumentTypeId RegisteredTypes::GetDocumentTypeId(const char name[]) const
    {
        for (auto i=_knownDocumentTypes.begin(); i!=_knownDocumentTypes.end(); ++i) {
            if (!XlCompareString(i->_name.c_str(), name)) {
                return 1+(ObjectTypeId)std::distance(_knownDocumentTypes.begin(), i);
            }
        }
        for (auto i=_types.cbegin(); i!=_types.cend(); ++i) {
            auto id = (*i)->GetDocumentTypeId(name);
            if (id != 0) {
                KnownType t;
                t._owner = *i;
                t._mappedTypeId = id;
                t._name = name;
                _knownDocumentTypes.push_back(std::move(t));
                return 1+(ObjectTypeId)(_knownDocumentTypes.size()-1);
            }
        }
        return 0;
    }

    PropertyId RegisteredTypes::GetPropertyId(ObjectTypeId objType, const char name[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetPropertyId(objType, name);
        }
        return 0;
    }

    ChildListId RegisteredTypes::GetChildListId(ObjectTypeId objType, const char name[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetChildListId(objType, name);
        }
        return 0;
    }

    void RegisteredTypes::RegisterType(std::shared_ptr<IObjectType> type)
    {
        _types.push_back(std::move(type));
    }
    RegisteredTypes::RegisteredTypes() {}
    RegisteredTypes::~RegisteredTypes() {}
    

}}

