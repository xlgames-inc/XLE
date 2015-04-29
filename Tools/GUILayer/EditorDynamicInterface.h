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
    class EditorScene;
    namespace EditorDynamicInterface
    {
        using ObjectTypeId = uint32;
        using DocumentTypeId = uint32;
        using PropertyId = uint32;
        using ChildListId = uint32;

        using DocumentId = uint64;
        using ObjectId = uint64;

        class IObjectType
        {
        public:
            virtual DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const = 0;
            virtual bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const = 0;

            class PropertyInitializer
            {
            public:
                PropertyId _prop;
                const void* _src;
                unsigned _elementType;
                unsigned _arrayCount;
            };

            virtual ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId objType) const = 0;
            virtual bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const PropertyInitializer initializers[], size_t initializerCount) const = 0;
            virtual bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const = 0;
            virtual bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const PropertyInitializer initializers[], size_t initializerCount) const = 0;
            virtual bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, unsigned* destSize) const = 0;
            virtual bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const = 0;

            virtual ObjectTypeId GetTypeId(const char name[]) const = 0;
            virtual DocumentTypeId GetDocumentTypeId(const char name[]) const = 0;
            virtual PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const = 0;
            virtual ChildListId GetChildListId(ObjectTypeId type, const char name[]) const = 0;

            virtual ~IObjectType();
        };

        class RegisteredTypes : public IObjectType
        {
        public:
            DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
            bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;
            
            ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId objType) const;
            bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const PropertyInitializer initializers[], size_t initializerCount) const;
            bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
            bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const PropertyInitializer initializers[], size_t initializerCount) const;
            bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, unsigned* destSize) const;
            bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const;

            ObjectTypeId GetTypeId(const char name[]) const;
            DocumentTypeId GetDocumentTypeId(const char name[]) const;
            PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
            ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

            uint32 MapTypeId(ObjectTypeId type, const IObjectType& owner);

            void RegisterType(std::shared_ptr<IObjectType> type);
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

