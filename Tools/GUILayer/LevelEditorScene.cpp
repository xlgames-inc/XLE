// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../../SceneEngine/SceneParser.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../Utility/StringFormat.h"
#include "../../Core/Types.h"
#include <memory>

namespace GUILayer
{
    template<typename T> using AutoToShared = clix::auto_ptr<std::shared_ptr<T>>;

    // Many level editors work around a structure of objects and attributes.
    // That is, the scene is composed of a hierachy of objects, and each object
    // as some type, and a set of attributes. In the case of the SonyWWS editor,
    // this structure is backed by a xml based DOM.
    // 
    // It's handy because it's simple and flexible. And we can store just about
    // anything like this.
    // 
    // When working with a level editor like this, we need a some kind of dynamic
    // "scene" object. This scene should react to basic commands from the editor:
    //
    //      * Create/destroy object (with given fixed type)
    //      * Set object attribute
    //      * Set object parent
    //
    // We also want to build in a "document" concept. Normally a document should
    // represent a single target file (eg a level file or some settings file).
    // Every object should belong to a single object, and all of the objects in
    // a single document should usually be part of one large hierarchy.
    //
    // For convenience when working with the SonyWWS editor, we want to be able to
    // pre-register common strings (like type names and object property names).
    // It might be ok just to use hash values for all of these cases. It depends on
    // whether we want to validate the names when they are first registered.

    class EditorScene
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;

        EditorScene();
    };

    EditorScene::EditorScene()
    {
        _placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
            SceneEngine::WorldPlacementsConfig(),
            std::shared_ptr<RenderCore::Assets::IModelFormat>(), Float2(0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
    }

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

            virtual ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId objType) const = 0;
            virtual bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const char initializer[]) const = 0;
            virtual bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const = 0;
            virtual bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const char stringForm[]) const = 0;

            virtual ObjectTypeId GetTypeId(const char name[]) const = 0;
            virtual DocumentTypeId GetDocumentTypeId(const char name[]) const = 0;
            virtual PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const = 0;
            virtual ChildListId GetChildListId(ObjectTypeId type, const char name[]) const = 0;

            virtual ~IObjectType();
        };

        IObjectType::~IObjectType() {}

        class RegisteredTypes : public IObjectType
        {
        public:
            DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
            bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;
            
            ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId objType) const;
            bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, const char initializer[]) const;
            bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
            bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const char stringForm[]) const;

            ObjectTypeId GetTypeId(const char name[]) const;
            DocumentTypeId GetDocumentTypeId(const char name[]) const;
            PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
            ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

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

        DocumentId RegisteredTypes::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
        {
            if (docType > 0 && (docType-1) < _knownDocumentTypes.size()) {
                auto& reg = _knownDocumentTypes[docType];
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

        bool RegisteredTypes::SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const char stringForm[]) const
        {
            if (objType > 0 && (objType-1) < _knownObjectTypes.size()) {
                auto& reg = _knownObjectTypes[objType-1];
                return reg._owner->SetProperty(scene, doc, obj, reg._mappedTypeId, prop, stringForm);
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
    }


    class EditorSceneParser : public SceneEngine::ISceneParser
    {
    public:
    };


    using namespace EditorDynamicInterface;
    class PlacementObjectType : public IObjectType
    {
    public:
        DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
        bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

        ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
        bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const char initializer[]) const;
        bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
        bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, const char stringForm[]) const;

        ObjectTypeId GetTypeId(const char name[]) const;
        DocumentTypeId GetDocumentTypeId(const char name[]) const;
        PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
        ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

        PlacementObjectType();
        ~PlacementObjectType();

        static const DocumentTypeId DocumentType_Placements = 1; //(DocumentTypeId)ConstHash64<'plac', 'emen', 'tsdo', 'c'>::Value;
        static const ObjectTypeId ObjectType_Placement = 1; // (ObjectTypeId)ConstHash64<'plac', 'emen', 't'>::Value;
        static const PropertyId Property_Transform = 100;
        static const PropertyId Property_Visible = 101;
    };

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
        newObj._model = "game/model/nature/bushtree/BushE.DAE";

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
        const char stringForm[]) const
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
                auto originalObject = transaction->GetObject(0);
                originalObject._localToWorld = *(const Float3x4*)stringForm;
                transaction->SetObject(0, originalObject);
                transaction->Commit();
            }
            return true;
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
        return 0;
    }

    ChildListId PlacementObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
    {
        return 0;
    }

    PlacementObjectType::PlacementObjectType() {}
    PlacementObjectType::~PlacementObjectType() {}


    public ref class EditorSceneManager
    {
    public:
        DocumentId CreateDocument(DocumentTypeId docType, System::String^ initializer);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId type, System::String^ initializer);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* data);

        ObjectTypeId GetTypeId(System::String^ name);
        DocumentTypeId GetDocumentTypeId(System::String^ name);
        PropertyId GetPropertyId(ObjectTypeId type, System::String^ name);
        ChildListId GetChildListId(ObjectTypeId type, System::String^ name);

        EditorSceneManager();

    protected:
        AutoToShared<EditorScene> _scene;
        AutoToShared<EditorDynamicInterface::RegisteredTypes> _dynInterface;
    };


    DocumentId EditorSceneManager::CreateDocument(DocumentTypeId docType, System::String^ initializer) 
        { return (*_dynInterface)->CreateDocument(**_scene, docType, clix::marshalString<clix::E_UTF8>(initializer).c_str()); }
    bool EditorSceneManager::DeleteDocument(DocumentId doc, DocumentTypeId docType)
        { return (*_dynInterface)->DeleteDocument(**_scene, doc, docType); }

    ObjectId EditorSceneManager::AssignObjectId(DocumentId doc, ObjectTypeId type)
        { return (*_dynInterface)->AssignObjectId(**_scene, doc, type); }

    bool EditorSceneManager::CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId type, System::String^ initializer)
        { return (*_dynInterface)->CreateObject(**_scene, doc, obj, type, clix::marshalString<clix::E_UTF8>(initializer).c_str()); }

    bool EditorSceneManager::DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return (*_dynInterface)->DeleteObject(**_scene, doc, obj, objType); }

    bool EditorSceneManager::SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* data)
        { return (*_dynInterface)->SetProperty(**_scene, doc, obj, type, prop, (const char*)data); }

    ObjectTypeId EditorSceneManager::GetTypeId(System::String^ name) { return (*_dynInterface)->GetTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    DocumentTypeId EditorSceneManager::GetDocumentTypeId(System::String^ name) { return (*_dynInterface)->GetDocumentTypeId(clix::marshalString<clix::E_UTF8>(name).c_str()); }
    PropertyId EditorSceneManager::GetPropertyId(ObjectTypeId type, System::String^ name) { return (*_dynInterface)->GetPropertyId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }
    ChildListId EditorSceneManager::GetChildListId(ObjectTypeId type, System::String^ name) { return (*_dynInterface)->GetChildListId(type, clix::marshalString<clix::E_UTF8>(name).c_str()); }

    template<typename Type>
        void InitAutoToShared(AutoToShared<Type>% obj)
        {
            obj.reset(new std::shared_ptr<Type>(std::make_shared<Type>()));
        }

    EditorSceneManager::EditorSceneManager()
    {
        InitAutoToShared(_scene);
        InitAutoToShared(_dynInterface);
        (*_dynInterface)->RegisterType(std::make_shared<PlacementObjectType>());
    }
}