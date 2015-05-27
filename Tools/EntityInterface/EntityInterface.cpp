// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EntityInterface.h"
#include "../../Utility/StringUtils.h"

namespace EntityInterface
{
    IEntityInterface::~IEntityInterface() {}

    IEntityInterface* Switch::GetInterface(
        Identifier& translatedId, 
        const Identifier& inputId) const
    {
        if (inputId.ObjectType() > 0 && (inputId.ObjectType()-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[inputId.ObjectType()-1];
            translatedId = Identifier(inputId.Document(), inputId.Object(), reg._mappedTypeId);
            return reg._owner.get();
        }
        return nullptr;
    }

    DocumentId Switch::CreateDocument(DocumentTypeId docType, const char initializer[]) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->CreateDocument(reg._mappedTypeId, initializer);
        }
        return 0;
    }

    bool Switch::DeleteDocument(DocumentId doc, DocumentTypeId docType) const
    {
        if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
            auto& reg = _knownDocumentTypes[docType-1];
            return reg._owner->DeleteDocument(doc, reg._mappedTypeId);
        }
        return false;
    }
            
    ObjectId Switch::AssignObjectId(DocumentId doc, ObjectTypeId objType) const
    {
            // Our object id is must be unique for all objects of all types.
            // Each sub-entity interface might have it's own algorithm for generating
            // ids. But there is no guarantee that we won't get conflicts generated
            // by 2 different sub-interfaces. So let's avoid calling the sub-interfaces
            // and just generate an id that is unique for this entire switch.
        return _nextObjectId++;
    }

    ObjectTypeId Switch::GetTypeId(const char name[]) const
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

    DocumentTypeId Switch::GetDocumentTypeId(const char name[]) const
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

    PropertyId Switch::GetPropertyId(ObjectTypeId objType, const char name[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetPropertyId(reg._mappedTypeId, name);
        }
        return 0;
    }

    ChildListId Switch::GetChildListId(ObjectTypeId objType, const char name[]) const
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
            auto& reg = _knownObjectTypes[objType-1];
            return reg._owner->GetChildListId(reg._mappedTypeId, name);
        }
        return 0;
    }

    uint32 Switch::MapTypeId(ObjectTypeId objType, const IEntityInterface& owner)
    {
        if (objType > 0 && (objType-1) < _knownObjectTypes.size())
            if (_knownObjectTypes[objType-1]._owner.get() == &owner)
                return _knownObjectTypes[objType-1]._mappedTypeId;
        return 0;
    }

    void Switch::RegisterType(std::shared_ptr<IEntityInterface> type)
    {
        _types.push_back(std::move(type));
    }

    Switch::Switch() : _nextObjectId(1) {}
    Switch::~Switch() {}
    

}


