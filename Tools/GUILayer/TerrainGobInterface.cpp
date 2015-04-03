// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainGobInterface.h"
#include "LevelEditorScene.h"
#include "ExportedNativeTypes.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Assets/TerrainFormat.h"
#include "../../Tools/ToolsRig/TerrainManipulators.h"
#include "../../Utility/StringFormat.h"
#include "../../Math/Transformations.h"

namespace GUILayer { namespace EditorDynamicInterface
{

	DocumentId TerrainObjectType::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
	{
		return 0;
	}

	bool TerrainObjectType::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
	{
		return false;
	}

	ObjectId TerrainObjectType::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const
	{
		if (type != ObjectType_Terrain) { assert(0); return 0; }
		return 1;
	}

	bool TerrainObjectType::CreateObject(
		EditorScene& scene, DocumentId doc,
		ObjectId obj, ObjectTypeId type,
		const char initializer[]) const
	{
		if (type != ObjectType_Terrain) { assert(0); return false; }
		scene._terrainManager.reset();
		scene._terrainManipulators.clear();
			//	We don't actually create until we set the "basedir" property
			//	this is just a short-cut to avoid extra work.
		return true;
	}

	bool TerrainObjectType::DeleteObject(
		EditorScene& scene, DocumentId doc,
		ObjectId obj, ObjectTypeId type) const
	{
		if (type != ObjectType_Terrain) { assert(0); return false; }
		scene._terrainManager.reset();
		scene._terrainManipulators.clear();
		return true;
	}

	bool TerrainObjectType::SetProperty(
		EditorScene& scene, DocumentId doc, ObjectId obj,
		ObjectTypeId type, PropertyId prop,
		const void* src, size_t srcSize) const
	{
		if (type != ObjectType_Terrain) { assert(0); return false; }
		if (prop != Property_BaseDir) { assert(0); return false; }

		scene._terrainManager.reset();
		scene._terrainManipulators.clear();

		SceneEngine::TerrainConfig cfg((const ::Assets::ResChar*)src);
		scene._terrainManager = std::make_shared<SceneEngine::TerrainManager>(
			cfg, std::make_unique<RenderCore::Assets::TerrainFormat>(),
			SceneEngine::GetBufferUploads(),
			Int2(0, 0), cfg._cellCount);

		// we must create all of the manipulator objects after creating the terrain (because they are associated)
        auto manip = ToolsRig::CreateTerrainManipulators(scene._terrainManager);
        for (auto& t : manip) {
            scene._terrainManipulators.push_back(
                EditorScene::RegisteredManipulator(t->GetName(), std::move(t)));
        }

		
		return false;
	}

	bool TerrainObjectType::GetProperty(
		EditorScene& scene, DocumentId doc, ObjectId obj,
		ObjectTypeId type, PropertyId prop,
		void* dest, size_t* destSize) const
	{
		assert(0);		
		return false;
	}

	ObjectTypeId TerrainObjectType::GetTypeId(const char name[]) const
	{
		if (!XlCompareString(name, "Terrain")) return ObjectType_Terrain;
		return 0;
	}

	DocumentTypeId TerrainObjectType::GetDocumentTypeId(const char name[]) const
	{
		return 0;
	}

	PropertyId TerrainObjectType::GetPropertyId(ObjectTypeId type, const char name[]) const
	{
		if (!XlCompareString(name, "basedir")) return Property_BaseDir;
		return 0;
	}

	ChildListId TerrainObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
	{
		return 0;
	}

	TerrainObjectType::TerrainObjectType() {}
	TerrainObjectType::~TerrainObjectType() {}

}}


