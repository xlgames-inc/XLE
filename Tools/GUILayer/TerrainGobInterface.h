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
#include <vector>

namespace Tools { class IManipulator; }
namespace SceneEngine { class TerrainManager; class TerrainMaterialScaffold; }

namespace GUILayer { namespace EditorDynamicInterface
{
    class TerrainObjectType : public IObjectType
	{
	public:
		DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
		bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

		ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const PropertyInitializer initializers[], size_t initializerCount) const;
		bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
		bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const PropertyInitializer initializers[], size_t initializerCount) const;
		bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, unsigned* destSize) const;
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

        bool SetTerrainProperty(EditorScene& scene, const PropertyInitializer& prop) const;
	};

    class FlexObjectType;
}}

namespace GUILayer
{
    class TerrainGob
    {
    public:
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;

        void SetBaseDir(const ucs2 dir[], unsigned length);
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

    namespace Internal { void RegisterTerrainFlexObjects(EditorDynamicInterface::FlexObjectType& flexSys); }
}
