// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "LevelEditorScene.h"
#include "EditorDynamicInterface.h"
#include "PlacementsInterface.h"
#include "CLIXAutoPtr.h"
#include "MarshalString.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../SceneEngine/SceneParser.h"
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

///////////////////////////////////////////////////////////////////////////////////////////////////

    EditorScene::EditorScene()
    {
        _placementsManager = std::make_shared<SceneEngine::PlacementsManager>(
            SceneEngine::WorldPlacementsConfig(),
            std::shared_ptr<RenderCore::Assets::IModelFormat>(), Float2(0.f, 0.f));
        _placementsEditor = _placementsManager->CreateEditor();
    }

    class EditorSceneParser : public SceneEngine::ISceneParser
    {
    public:
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    using namespace EditorDynamicInterface;
    public ref class EditorSceneManager
    {
    public:
        DocumentId CreateDocument(DocumentTypeId docType, System::String^ initializer);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType, System::String^ initializer);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* data);

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

    bool EditorSceneManager::CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType, System::String^ initializer)
        { return (*_dynInterface)->CreateObject(**_scene, doc, obj, objType, clix::marshalString<clix::E_UTF8>(initializer).c_str()); }

    bool EditorSceneManager::DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType)
        { return (*_dynInterface)->DeleteObject(**_scene, doc, obj, objType); }

    bool EditorSceneManager::SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* data)
        { return (*_dynInterface)->SetProperty(**_scene, doc, obj, objType, prop, (const char*)data); }

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
        (*_dynInterface)->RegisterType(std::make_shared<EditorDynamicInterface::PlacementObjectType>());
    }
}