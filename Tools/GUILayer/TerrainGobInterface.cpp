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

namespace GUILayer
{
    TerrainGob::RegisteredManipulator::~RegisteredManipulator() {}

    void TerrainGob::SetBaseDir(const Assets::ResChar dir[])
    {
        _terrainManager.reset();
		_terrainManipulators.clear();

        ::Assets::ResChar buffer[MaxPath];
        ucs2_2_utf8((const ucs2*)dir, XlStringLen((const ucs2*)dir), (utf8*)buffer, dimof(buffer));

		SceneEngine::TerrainConfig cfg(buffer);
		_terrainManager = std::make_shared<SceneEngine::TerrainManager>(
			cfg, std::make_unique<RenderCore::Assets::TerrainFormat>(),
			SceneEngine::GetBufferUploads(),
			Int2(0, 0), cfg._cellCount,
            _terrainOffset);

		// we must create all of the manipulator objects after creating the terrain (because they are associated)
        auto manip = ToolsRig::CreateTerrainManipulators(_terrainManager);
        for (auto& t : manip) {
            _terrainManipulators.push_back(
                RegisteredManipulator(t->GetName(), std::move(t)));
        }
    }

    void TerrainGob::SetOffset(const Float3& offset)
    {
        _terrainOffset = offset;
        if (_terrainManager) {
            _terrainManager->SetWorldSpaceOrigin(offset);
        }
    }

    TerrainGob::TerrainGob()
    {
            //	We don't actually create the terrain until we set the 
            //  "basedir" property
			//	this is just a short-cut to avoid extra work.
        _terrainOffset = Float3(0.f, 0.f, 0.f);
    }

    TerrainGob::~TerrainGob()
    {}
}

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
		scene._terrainGob = std::make_unique<TerrainGob>();
		return true;
	}

	bool TerrainObjectType::DeleteObject(
		EditorScene& scene, DocumentId doc,
		ObjectId obj, ObjectTypeId type) const
	{
		if (type != ObjectType_Terrain) { assert(0); return false; }
		scene._terrainGob.reset();
		return true;
	}

	bool TerrainObjectType::SetProperty(
		EditorScene& scene, DocumentId doc, ObjectId obj,
		ObjectTypeId type, PropertyId prop,
		const void* src, size_t srcSize) const
	{
		if (type != ObjectType_Terrain) { assert(0); return false; }

        if (!scene._terrainGob) {
            assert(0);
            return false;
        }
		
        if (prop == Property_BaseDir) {
            scene._terrainGob->SetBaseDir((const Assets::ResChar*)src);
            return true;
        } else if (prop == Property_Offset) {
            scene._terrainGob->SetOffset(*(const Float3*)src);
            return true;
        }

		assert(0);
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
        if (!XlCompareString(name, "offset")) return Property_Offset;
		return 0;
	}

	ChildListId TerrainObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
	{
		return 0;
	}

	TerrainObjectType::TerrainObjectType() {}
	TerrainObjectType::~TerrainObjectType() {}

}}


