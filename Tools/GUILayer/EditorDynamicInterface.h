// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Core/Types.h"
#include <memory>
#include <vector>

namespace GUILayer
{
    namespace EditorDynamicInterface
    {
        using ObjectTypeId = uint32;
        using DocumentTypeId = uint32;
        using PropertyId = uint32;
        using ChildListId = uint32;

        using DocumentId = uint64;
        using ObjectId = uint64;

        class PropertyInitializer
        {
        public:
            PropertyId _prop;
            const void* _src;
            unsigned _elementType;
            unsigned _arrayCount;
        };

        class Identifier
        {
        public:
            DocumentId Document() const         { return _doc; }
            ObjectTypeId ObjectType() const     { return _objType; }
            ObjectId Object() const             { return _obj; }

            Identifier(DocumentId doc, ObjectId obj, ObjectTypeId objType) : _doc(doc), _obj(obj), _objType(objType) {}
            Identifier() : _doc(DocumentId(0)), _obj(ObjectId(0)), _objType(ObjectTypeId(0)) {}
        protected:
            DocumentId _doc;
            ObjectId _obj;
            ObjectTypeId _objType;
        };

        class IObjectType
        {
        public:
            virtual DocumentId CreateDocument(DocumentTypeId docType, const char initializer[]) const = 0;
            virtual bool DeleteDocument(DocumentId doc, DocumentTypeId docType) const = 0;

            virtual ObjectId AssignObjectId(DocumentId doc, ObjectTypeId objType) const = 0;
            virtual bool CreateObject(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount) const = 0;
            virtual bool DeleteObject(const Identifier& id) const = 0;
            virtual bool SetProperty(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount) const = 0;
            virtual bool GetProperty(const Identifier& id, PropertyId prop, void* dest, unsigned* destSize) const = 0;
            virtual bool SetParent(const Identifier& child, const Identifier& parent, int insertionPosition) const = 0;

            virtual ObjectTypeId GetTypeId(const char name[]) const = 0;
            virtual DocumentTypeId GetDocumentTypeId(const char name[]) const = 0;
            virtual PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const = 0;
            virtual ChildListId GetChildListId(ObjectTypeId type, const char name[]) const = 0;

            virtual ~IObjectType();
        };

        class RegisteredTypes
        {
        public:
            IObjectType* GetInterface(
                Identifier& translatedId, 
                const Identifier& inputId) const;

            DocumentId  CreateDocument(DocumentTypeId docType, const char initializer[]) const;
            bool        DeleteDocument(DocumentId doc, DocumentTypeId docType) const;
            ObjectId    AssignObjectId(DocumentId doc, ObjectTypeId objType) const;

            ObjectTypeId    GetTypeId(const char name[]) const;
            DocumentTypeId  GetDocumentTypeId(const char name[]) const;
            PropertyId      GetPropertyId(ObjectTypeId type, const char name[]) const;
            ChildListId     GetChildListId(ObjectTypeId type, const char name[]) const;

            uint32  MapTypeId(ObjectTypeId type, const IObjectType& owner);
            void    RegisterType(std::shared_ptr<IObjectType> type);

            RegisteredTypes();
            ~RegisteredTypes();
        protected:
            std::vector<std::shared_ptr<IObjectType>> _types;

            class KnownType
            {
            public:
                std::shared_ptr<IObjectType> _owner;
                std::string _name;
                uint32 _mappedTypeId;
            };
            mutable std::vector<KnownType> _knownObjectTypes;
            mutable std::vector<KnownType> _knownDocumentTypes;
        };
    }
}

