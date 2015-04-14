// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "ManipulatorUtils.h"
#include "EditorDynamicInterface.h"
#include "CLIXAutoPtr.h"
#include "../../Assets/Assets.h"
#include "../../Math/Vector.h"
#include "../../Utility/ParameterBox.h"
#include <vector>
#include <functional>

namespace Tools { class IManipulator; }
namespace SceneEngine { class TerrainManager; class TerrainMaterialScaffold; }

namespace GUILayer { namespace EditorDynamicInterface
{
    class FlexObjectType : public IObjectType
    {
    public:
        DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
		bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

		ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, const char initializer[]) const;
		bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
		bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, const void* src, unsigned elementType, unsigned arrayCount) const;
		bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, void* dest, size_t* destSize) const;
        bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const;

		ObjectTypeId GetTypeId(const char name[]) const;
		DocumentTypeId GetDocumentTypeId(const char name[]) const;
		PropertyId GetPropertyId(ObjectTypeId typeId, const char name[]) const;
		ChildListId GetChildListId(ObjectTypeId typeId, const char name[]) const;

        using OnChangeDelegate = std::function<void(const FlexObjectType& flexSys, DocumentId, ObjectId, ObjectTypeId)>;
        bool RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange);

        class Object
        {
        public:
            ObjectId _id;
            DocumentId _doc;
            ObjectTypeId _type;

            ParameterBox _properties;
            std::vector<ObjectId> _children;
            ObjectId _parent;
        };
        const Object* GetObject(DocumentId doc, ObjectId obj) const;

		FlexObjectType();
		~FlexObjectType();

    protected:
        mutable ObjectId _nextObjectId;
        mutable std::vector<Object> _objects;

        class RegisteredObjectType
        {
        public:
            std::string _name;
            std::vector<std::string> _properties;
            std::vector<std::string> _childLists;

            std::vector<OnChangeDelegate> _onChange;

            RegisteredObjectType(const std::string& name) : _name(name) {}
        };
        mutable std::vector<std::pair<ObjectTypeId, RegisteredObjectType>> _registeredObjectTypes;

        mutable ObjectTypeId _nextObjectTypeId;

        RegisteredObjectType* GetObjectType(ObjectTypeId id) const;
        void InvokeOnChange(RegisteredObjectType& type, Object& obj) const;

        Object* GetObjectInt(DocumentId doc, ObjectId obj) const;
    };

    class TerrainObjectType : public IObjectType
	{
	public:
		DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
		bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

		ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const char initializer[]) const;
		bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
		bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, const void* src, unsigned elementType, unsigned arrayCount) const;
		bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, size_t* destSize) const;
        bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const;

		ObjectTypeId GetTypeId(const char name[]) const;
		DocumentTypeId GetDocumentTypeId(const char name[]) const;
		PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
		ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

		TerrainObjectType();
		~TerrainObjectType();

    private:
		static const ObjectTypeId ObjectType_Terrain = 1;
		static const PropertyId Property_BaseDir = 200;
        static const PropertyId Property_Offset = 201;
	};
}}

namespace GUILayer
{
    class TerrainGob
    {
    public:
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;

        void SetBaseDir(const Assets::ResChar dir[]);
        void SetOffset(const Float3& offset);

        ::Assets::DivergentAsset<SceneEngine::TerrainMaterialScaffold>& GetMaterial();

        TerrainGob();
        ~TerrainGob();

    protected:
        Float3 _terrainOffset;
    };

    class TerrainManipulatorsPimpl
    {
    public:
        class RegisteredManipulator
		{
		public:
			std::string _name;
			std::shared_ptr<ToolsRig::IManipulator> _manipulator;
			RegisteredManipulator(
				const std::string& name,
				std::shared_ptr<ToolsRig::IManipulator> manipulator)
				: _name(name), _manipulator(std::move(manipulator))
			{}
			RegisteredManipulator() {}
			~RegisteredManipulator();
		};
		std::vector<RegisteredManipulator> _terrainManipulators;
    };

    ref class TerrainManipulators : public IManipulatorSet
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) override;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() override;

        TerrainManipulators(std::shared_ptr<SceneEngine::TerrainManager> terrain);
        ~TerrainManipulators();
    protected:
        clix::auto_ptr<TerrainManipulatorsPimpl> _pimpl;
    };
}
