// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementEntities.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/UTFUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Math/Transformations.h"

namespace EntityInterface
{
    static const DocumentTypeId DocumentType_Placements = 1;
    static const ObjectTypeId ObjectType_Placement = 1;
    static const PropertyId Property_Transform = 100;
    static const PropertyId Property_Visible = 101;
    static const PropertyId Property_Model = 102;
    static const PropertyId Property_Material = 103;
    static const PropertyId Property_Bounds = 104;
    static const PropertyId Property_LocalBounds = 105;


    DocumentId PlacementEntities::CreateDocument(DocumentTypeId docType, const char initializer[])
    {
        if (docType != DocumentType_Placements) { assert(0); return 0; }

        StringMeld<MaxPath, ::Assets::ResChar> meld;
        meld << "[dyn] " << initializer << (_cellCounter++);

        return (DocumentId)_editor->CreateCell(
            *_manager,
            meld,  Float2(-1000.f, -1000.f), Float2( 1000.f,  1000.f));
    }

    bool PlacementEntities::DeleteDocument(DocumentId doc, DocumentTypeId docType)
    {
        if (docType != DocumentType_Placements) { assert(0); return false; }
        return _editor->RemoveCell(*_manager, doc);
    }

    ObjectId PlacementEntities::AssignObjectId(DocumentId doc, ObjectTypeId type) const
    {
        if (type != ObjectType_Placement) { assert(0); return 0; }
        return _editor->GenerateObjectGUID();
    }

    static bool SetObjProperty(
        SceneEngine::PlacementsEditor::ObjTransDef& obj, 
        const PropertyInitializer& prop)
    {
        if (prop._prop == Property_Transform) {
                // note -- putting in a transpose here, because the level editor matrix
                //          math uses a transposed form
            if (prop._elementType == (unsigned)ImpliedTyping::TypeCat::Float && prop._arrayCount >= 16) {
                obj._localToWorld = AsFloat3x4(Transpose(*(const Float4x4*)prop._src));
                return true;
            }

        } else if (prop._prop == Property_Model || prop._prop == Property_Material) {
            Assets::ResChar buffer[MaxPath];
            ucs2_2_utf8(
                (const ucs2*)prop._src, prop._arrayCount,
                (utf8*)buffer, dimof(buffer));

            if (prop._prop == Property_Model) {
                obj._model = buffer;
            } else {
                obj._material = buffer;
            }
            return true;
        }
        return false;
    }

    bool PlacementEntities::CreateObject(
        const Identifier& id, 
        const PropertyInitializer initializers[], size_t initializerCount)
    {
        if (id.ObjectType() != ObjectType_Placement) { assert(0); return false; }

        SceneEngine::PlacementsEditor::ObjTransDef newObj;
        newObj._localToWorld = Identity<decltype(newObj._localToWorld)>();
        newObj._model = "game/model/nature/bushtree/BushE";
        newObj._material = "game/model/nature/bushtree/BushE";

        auto guid = SceneEngine::PlacementGUID(id.Document(), id.Object());
        auto transaction = _editor->Transaction_Begin(nullptr, nullptr);
        if (transaction->Create(guid, newObj)) {

            if (initializerCount) {
                auto originalObject = transaction->GetObject(0);

                bool result = false;
                for (size_t c=0; c<initializerCount; ++c) 
                    result |= SetObjProperty(originalObject, initializers[c]);

                if (result)
                    transaction->SetObject(0, originalObject);
            }

            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementEntities::DeleteObject(const Identifier& id)
    {
        if (id.ObjectType() != ObjectType_Placement) { assert(0); return false; }

        auto guid = SceneEngine::PlacementGUID(id.Document(), id.Object());
        auto transaction = _editor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            transaction->Delete(0);
            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementEntities::SetProperty(
        const Identifier& id,
        const PropertyInitializer initializers[], size_t initializerCount)
    {
            // find the object, and set the given property (as per the new value specified in the string)
            //  We need to create a transaction, make the change and then commit it back.
            //  If the transaction returns no results, then we must have got a bad object or document id.
        if (id.ObjectType() != ObjectType_Placement) { assert(0); return false; }

            // note --  This object search is quite slow! We might need a better way to
            //          record a handle to the object. Perhaps the "ObjectId" should not
            //          match the actual placements guid. Some short-cut will probably be
            //          necessary given that we could get there several thousand times during
            //          startup for an average scene.
        auto guid = SceneEngine::PlacementGUID(id.Document(), id.Object());
        auto transaction = _editor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction && transaction->GetObjectCount()==1) {
            auto originalObject = transaction->GetObject(0);

            bool result = false;
            for (size_t c=0; c<initializerCount; ++c) {
                result |= SetObjProperty(originalObject, initializers[c]);
            }

            if (result) {
                transaction->SetObject(0, originalObject);
                transaction->Commit();
                return true;
            }
        }

        return false;
    }

    bool PlacementEntities::GetProperty(
        const Identifier& id, PropertyId prop, 
        void* dest, unsigned* destSize) const
    {
        if (id.ObjectType() != ObjectType_Placement) { assert(0); return false; }
        if (prop != Property_Transform && prop != Property_Visible
            && prop != Property_Bounds && prop != Property_LocalBounds) { assert(0); return false; }
        assert(destSize);

        typedef std::pair<Float3, Float3> BoundingBox;

        auto guid = SceneEngine::PlacementGUID(id.Document(), id.Object());
        auto transaction = _editor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            if (prop == Property_Transform) {
                if (*destSize >= sizeof(Float4x4)) {
                    auto originalObject = transaction->GetObject(0);
                        // note -- putting in a transpose here, because the level editor matrix
                        //          math uses a transposed form
                    *(Float4x4*)dest = Transpose(AsFloat4x4(originalObject._localToWorld));
                    return true;
                }
                *destSize = sizeof(Float4x4);
            } else if (prop == Property_Bounds) {
                if (*destSize >= sizeof(BoundingBox)) {
                    *(BoundingBox*)dest = transaction->GetWorldBoundingBox(0);
                    return true;
                }
                *destSize = sizeof(BoundingBox);
            } else if (prop == Property_LocalBounds) {
                if (*destSize >= sizeof(BoundingBox)) {
                    *(BoundingBox*)dest = transaction->GetLocalBoundingBox(0);
                    return true;
                }
                *destSize = sizeof(BoundingBox);
            }
        }

        return false;
    }

    bool PlacementEntities::SetParent(const Identifier& child, const Identifier& parent, int insertionPosition)
    {
        return false;
    }

    ObjectTypeId PlacementEntities::GetTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementObject")) return ObjectType_Placement;
        return 0;
    }

    DocumentTypeId PlacementEntities::GetDocumentTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementsDocument")) return DocumentType_Placements;
        return 0;
    }

    PropertyId PlacementEntities::GetPropertyId(ObjectTypeId type, const char name[]) const
    {
        if (!XlCompareString(name, "transform"))    return Property_Transform;
        if (!XlCompareString(name, "visible"))      return Property_Visible;
        if (!XlCompareString(name, "model"))        return Property_Model;
        if (!XlCompareString(name, "material"))     return Property_Material;
        if (!XlCompareString(name, "Bounds"))       return Property_Bounds;
        if (!XlCompareString(name, "LocalBounds"))  return Property_LocalBounds;
        return 0;
    }

    ChildListId PlacementEntities::GetChildListId(ObjectTypeId type, const char name[]) const
    {
        return 0;
    }

    PlacementEntities::PlacementEntities(
        std::shared_ptr<SceneEngine::PlacementsManager> manager,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    : _manager(std::move(manager))
    , _editor(std::move(editor))
    , _cellCounter(0) 
    {}

    PlacementEntities::~PlacementEntities() {}

}
