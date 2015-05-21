// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../EntityInterface/EntityInterface.h"
#include "CLIXAutoPtr.h"

using namespace System::Collections::Generic;
namespace EntityInterface { class Switch; }

namespace GUILayer
{
    public ref class EntityLayer
    {
    public:
                //// //// ////   G O B   I N T E R F A C E   //// //// ////
        using DocumentTypeId = EntityInterface::DocumentTypeId;
        using ObjectTypeId = EntityInterface::ObjectTypeId;
        using DocumentId = EntityInterface::DocumentId;
        using ObjectId = EntityInterface::ObjectId;
        using ObjectTypeId = EntityInterface::ObjectTypeId;
        using PropertyId = EntityInterface::PropertyId;
        using ChildListId = EntityInterface::ChildListId;

        DocumentId CreateDocument(DocumentTypeId docType);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        value struct PropertyInitializer
        {
            PropertyId _prop;
            const void* _src;
            unsigned _elementType;
            unsigned _arrayCount;

            PropertyInitializer(PropertyId prop, const void* src, unsigned elementType, unsigned arrayCount)
                : _prop(prop), _src(src), _elementType(elementType), _arrayCount(arrayCount) {}
        };

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType, IEnumerable<PropertyInitializer>^ initializers);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, IEnumerable<PropertyInitializer>^ initializers);
        bool GetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, unsigned* destSize);

        bool SetObjectParent(DocumentId doc, 
            ObjectId childId, ObjectTypeId childTypeId, 
            ObjectId parentId, ObjectTypeId parentTypeId, int insertionPosition);

        ObjectTypeId GetTypeId(System::String^ name);
        DocumentTypeId GetDocumentTypeId(System::String^ name);
        PropertyId GetPropertyId(ObjectTypeId type, System::String^ name);
        ChildListId GetChildListId(ObjectTypeId type, System::String^ name);

        EntityInterface::Switch& GetSwitch();

        EntityLayer(std::shared_ptr<EntityInterface::Switch> swtch);
        ~EntityLayer();
    protected:
        clix::shared_ptr<EntityInterface::Switch> _switch;
    };

}