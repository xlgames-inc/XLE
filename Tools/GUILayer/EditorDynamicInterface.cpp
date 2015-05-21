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

    IObjectType* RegisteredTypes::GetInterface(
        Identifier& translatedId, 
        const Identifier& inputId) const
    {
        if (inputId.Document() > 0 && (inputId.Document()-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[inputId.Document()-1];
            translatedId = Identifier(inputId.Document(), inputId.Object(), reg._mappedTypeId);
            return reg._owner.get();
        }
        return nullptr;
    }

    DocumentId RegisteredTypes::CreateDocument(DocumentTypeId docType, const char initializer[]) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->CreateDocument(reg._mappedTypeId, initializer);
        }
        return 0;
    }

    bool RegisteredTypes::DeleteDocument(DocumentId doc, DocumentTypeId docType) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->DeleteDocument(doc, reg._mappedTypeId);
        }
        return false;
    }
            
    ObjectId RegisteredTypes::AssignObjectId(DocumentId doc, ObjectTypeId objType) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->AssignObjectId(doc, reg._mappedTypeId);
        }
        return 0;
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
            return reg._owner->GetPropertyId(reg._mappedTypeId, name);
        }
        return 0;
    }

    ChildListId RegisteredTypes::GetChildListId(ObjectTypeId objType, const char name[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetChildListId(reg._mappedTypeId, name);
        }
        return 0;
    }

    uint32 RegisteredTypes::MapTypeId(ObjectTypeId objType, const IObjectType& owner)
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size())
            if (_knownObjectTypes[objType-1]._owner.get() == &owner)
                return _knownObjectTypes[objType-1]._mappedTypeId;
        return 0;
    }

    void RegisteredTypes::RegisterType(std::shared_ptr<IObjectType> type)
    {
        _types.push_back(std::move(type));
    }

    RegisteredTypes::RegisteredTypes() {}
    RegisteredTypes::~RegisteredTypes() {}
    

}}

