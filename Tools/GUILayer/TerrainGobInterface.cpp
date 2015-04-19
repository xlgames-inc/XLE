// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainGobInterface.h"
#include "LevelEditorScene.h"
#include "ExportedNativeTypes.h"
#include "MarshalString.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainMaterial.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../RenderCore/Assets/TerrainFormat.h"
#include "../../Tools/ToolsRig/TerrainManipulators.h"
#include "../../Tools/ToolsRig/IManipulator.h"

namespace GUILayer
{
    void TerrainGob::SetBaseDir(const Assets::ResChar dir[])
    {
        _terrainManager.reset();

        ::Assets::ResChar buffer[MaxPath];
        ucs2_2_utf8((const ucs2*)dir, XlStringLen((const ucs2*)dir), (utf8*)buffer, dimof(buffer));

		SceneEngine::TerrainConfig cfg(buffer);
        cfg._textureCfgName = "";
		_terrainManager = std::make_shared<SceneEngine::TerrainManager>(
			cfg, std::make_unique<RenderCore::Assets::TerrainFormat>(),
			SceneEngine::GetBufferUploads(),
			Int2(0, 0), cfg._cellCount,
            _terrainOffset);
    }

    void TerrainGob::SetOffset(const Float3& offset)
    {
        _terrainOffset = offset;
        if (_terrainManager) {
            _terrainManager->SetWorldSpaceOrigin(offset);
        }
    }

    ::Assets::DivergentAsset<SceneEngine::TerrainMaterialScaffold>& TerrainGob::GetMaterial()
    {
        return *Assets::GetDivergentAsset<SceneEngine::TerrainMaterialScaffold>();
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



    TerrainManipulatorsPimpl::RegisteredManipulator::~RegisteredManipulator() {}

	clix::shared_ptr<ToolsRig::IManipulator> TerrainManipulators::GetManipulator(System::String^ name)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(name);
		for (auto i : _pimpl->_terrainManipulators)
			if (i._name == nativeName) return clix::shared_ptr<ToolsRig::IManipulator>(i._manipulator);
		return clix::shared_ptr<ToolsRig::IManipulator>();
	}

	System::Collections::Generic::IEnumerable<System::String^>^ TerrainManipulators::GetManipulatorNames()
	{
		auto result = gcnew System::Collections::Generic::List<System::String^>();
		for (auto i : _pimpl->_terrainManipulators)
			result->Add(clix::marshalString<clix::E_UTF8>(i._name));
		return result;
	}

    TerrainManipulators::TerrainManipulators(std::shared_ptr<SceneEngine::TerrainManager> terrain)
    {
        _pimpl.reset(new TerrainManipulatorsPimpl);

        auto manip = ToolsRig::CreateTerrainManipulators(terrain);
        for (auto& t : manip) {
            _pimpl->_terrainManipulators.push_back(
                TerrainManipulatorsPimpl::RegisteredManipulator(t->GetName(), std::move(t)));
        }
    }

    TerrainManipulators::~TerrainManipulators() 
    {
        _pimpl.reset();
    }
}

namespace GUILayer { namespace EditorDynamicInterface
{

///////////////////////////////////////////////////////////////////////////////////////////////////

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
		if (type == ObjectType_Terrain) {
		    return 1;
        }
        static ObjectId incrementalId = 100;
        return incrementalId++;
	}

	bool TerrainObjectType::CreateObject(
		EditorScene& scene, DocumentId doc,
		ObjectId obj, ObjectTypeId type,
		const char initializer[]) const
	{
		if (type == ObjectType_Terrain) {
		    scene._terrainGob = std::make_unique<TerrainGob>();
		    return true;
        }
        return false;
	}

	bool TerrainObjectType::DeleteObject(
		EditorScene& scene, DocumentId doc,
		ObjectId obj, ObjectTypeId type) const
	{
		if (type == ObjectType_Terrain) {
		    scene._terrainGob.reset();
    		return true;
        }
        return false;
	}

	bool TerrainObjectType::SetProperty(
		EditorScene& scene, DocumentId doc, ObjectId obj,
		ObjectTypeId type, PropertyId prop,
		const void* src, unsigned elementType, unsigned arrayCount) const
	{
		if (type == ObjectType_Terrain) {
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
        // } else if (type == ObjectType_BaseTexture) {
        //     return false;
        // } else if (type == ObjectType_BaseTextureStrata) {
        //     return false;
        }

		// assert(0);
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

    bool TerrainObjectType::SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const
    {
        return false;
    }

	ObjectTypeId TerrainObjectType::GetTypeId(const char name[]) const
	{
		if (!XlCompareString(name, "Terrain")) return ObjectType_Terrain;
        // if (!XlCompareString(name, "TerrainBaseTexture")) return ObjectType_BaseTexture;
        // if (!XlCompareString(name, "TerrainBaseTextureStrata")) return ObjectType_BaseTextureStrata;
		return 0;
	}

	DocumentTypeId TerrainObjectType::GetDocumentTypeId(const char name[]) const
	{
		return 0;
	}

	PropertyId TerrainObjectType::GetPropertyId(ObjectTypeId type, const char name[]) const
	{
        if (type == ObjectType_Terrain) {
		    if (!XlCompareString(name, "basedir")) return Property_BaseDir;
            if (!XlCompareString(name, "offset")) return Property_Offset;
        }

        // if (type == ObjectType_BaseTexture) {
        //     if (!XlCompareString(name, "diffusedims")) return Property_DiffuseDims;
        //     if (!XlCompareString(name, "normaldims")) return Property_NormalDims;
        //     if (!XlCompareString(name, "paramdims")) return Property_ParamDims;
        // }
        // 
        // if (type == ObjectType_BaseTextureStrata) {
        //     if (!XlCompareString(name, "texture0")) return Property_Texture0;
        //     if (!XlCompareString(name, "texture1")) return Property_Texture1;
        //     if (!XlCompareString(name, "texture2")) return Property_Texture2;
        //     if (!XlCompareString(name, "mapping0")) return Property_Mapping0;
        //     if (!XlCompareString(name, "mapping1")) return Property_Mapping1;
        //     if (!XlCompareString(name, "mapping2")) return Property_Mapping2;
        // }

		return 0;
	}

	ChildListId TerrainObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
	{
		return 0;
	}

	TerrainObjectType::TerrainObjectType() {}
	TerrainObjectType::~TerrainObjectType() {}

}}


