// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsInterface.h"
#include "LevelEditorScene.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../Utility/StringFormat.h"
#include "../../Math/Transformations.h"

namespace GUILayer { namespace EditorDynamicInterface
{

    DocumentId PlacementObjectType::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
    {
        if (docType != DocumentType_Placements) { assert(0); return 0; }

        StringMeld<MaxPath, ::Assets::ResChar> meld;
        meld << "[dyn] " << initializer;

        return (DocumentId)scene._placementsEditor->CreateCell(
            *scene._placementsManager,
            meld,  Float2(-1000.f, -1000.f), Float2( 1000.f,  1000.f));
    }

    bool PlacementObjectType::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
    {
        if (docType != DocumentType_Placements) { assert(0); return false; }
        scene._placementsEditor->RemoveCell(*scene._placementsManager, doc);
        return true;
    }

    ObjectId PlacementObjectType::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const
    {
        if (type != ObjectType_Placement) { assert(0); return 0; }
        return scene._placementsEditor->GenerateObjectGUID();
    }

    bool PlacementObjectType::CreateObject(
        EditorScene& scene, DocumentId doc, 
        ObjectId obj, ObjectTypeId type, 
        const char initializer[]) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }

        SceneEngine::PlacementsEditor::ObjTransDef newObj;
        newObj._localToWorld = Identity<decltype(newObj._localToWorld)>();
        newObj._model = "game/model/nature/bushtree/BushE.dae";
        newObj._material = "game/model/nature/bushtree/BushE.dae";

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(nullptr, nullptr);
        if (transaction->Create(guid, newObj)) {
            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementObjectType::DeleteObject(
        EditorScene& scene, DocumentId doc, 
        ObjectId obj, ObjectTypeId type) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            transaction->Delete(0);
            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementObjectType::SetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, 
        ObjectTypeId type, PropertyId prop, 
        const void* src, size_t srcSize) const
    {
            // find the object, and set the given property (as per the new value specified in the string)
            //  We need to create a transaction, make the change and then commit it back.
            //  If the transaction returns no results, then we must have got a bad object or document id.
        if (type != ObjectType_Placement) { assert(0); return false; }
        if (prop != Property_Transform && prop != Property_Visible) { assert(0); return false; }

            // note --  This object search is quite slow! We might need a better way to
            //          record a handle to the object. Perhaps the "ObjectId" should not
            //          match the actual placements guid. Some short-cut will probably be
            //          necessary given that we could get there several thousand times during
            //          startup for an average scene.
        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            if (prop == Property_Transform) {
                    // note -- putting in a transpose here, because the level editor matrix
                    //          math uses a transposed form
                if (srcSize >= sizeof(Float4x4)) {
                    auto originalObject = transaction->GetObject(0);
                    originalObject._localToWorld = AsFloat3x4(Transpose(*(const Float4x4*)src));
                    transaction->SetObject(0, originalObject);
                    transaction->Commit();
                    return true;
                }
            }
        }

        return false;
    }

    bool PlacementObjectType::GetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, 
        ObjectTypeId type, PropertyId prop, 
        void* dest, size_t* destSize) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }
        if (prop != Property_Transform && prop != Property_Visible
            && prop != Property_Bounds && prop != Property_LocalBounds) { assert(0); return false; }
        assert(destSize);

        typedef std::pair<Float3, Float3> BoundingBox;

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
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

    ObjectTypeId PlacementObjectType::GetTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementObject")) return ObjectType_Placement;
        return 0;
    }

    DocumentTypeId PlacementObjectType::GetDocumentTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementsDocument")) return DocumentType_Placements;
        return 0;
    }

    PropertyId PlacementObjectType::GetPropertyId(ObjectTypeId type, const char name[]) const
    {
        if (!XlCompareString(name, "transform")) return Property_Transform;
        if (!XlCompareString(name, "visible")) return Property_Visible;
        if (!XlCompareString(name, "Bounds")) return Property_Bounds;
        if (!XlCompareString(name, "LocalBounds")) return Property_LocalBounds;
        return 0;
    }

    ChildListId PlacementObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
    {
        return 0;
    }

    PlacementObjectType::PlacementObjectType() {}
    PlacementObjectType::~PlacementObjectType() {}

}}


