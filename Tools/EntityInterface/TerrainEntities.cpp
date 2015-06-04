// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainEntities.h"
#include "RetainedEntities.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainConfig.h"
#include "../../SceneEngine/TerrainMaterial.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/TerrainFormat.h"

namespace EntityInterface
{
    static const ObjectTypeId ObjectType_Terrain = 1;
	static const PropertyId Property_CellsDir = 200;
    static const PropertyId Property_UberSurfaceDir = 201;
    static const PropertyId Property_Offset = 202;

///////////////////////////////////////////////////////////////////////////////////////////////////

	DocumentId TerrainEntities::CreateDocument(DocumentTypeId docType, const char initializer[]) { return 0; }
	bool TerrainEntities::DeleteDocument(DocumentId doc, DocumentTypeId docType) { return false; }

	ObjectId TerrainEntities::AssignObjectId(DocumentId doc, ObjectTypeId type) const
	{
		if (type == ObjectType_Terrain) {
		    return 1;
        }
        static ObjectId incrementalId = 100;
        return incrementalId++;
	}

	bool TerrainEntities::CreateObject(
		const Identifier& id,
		const PropertyInitializer initializers[], size_t initializerCount)
	{
		if (id.ObjectType() == ObjectType_Terrain) {
		    _terrainManager->Reset();
            _terrainManager->SetWorldSpaceOrigin(Float3(0.f, 0.f, 0.f));
            for (size_t c=0; c<initializerCount; ++c)
                SetTerrainProperty(initializers[c]);
		    return true;
        }
        return false;
	}

	bool TerrainEntities::DeleteObject(
		const Identifier& id)
	{
		if (id.ObjectType() == ObjectType_Terrain) {
		    _terrainManager->Reset();
            _uberSurfaceDir = ::Assets::rstring();
            _cellsDir = ::Assets::rstring();
    		return true;
        }
        return false;
	}

	bool TerrainEntities::SetProperty(
		const Identifier& id,
		const PropertyInitializer initializers[], size_t initializerCount)
	{
		if (id.ObjectType() == ObjectType_Terrain) {
            for (size_t c=0; c<initializerCount; ++c)
                SetTerrainProperty(initializers[c]);
            return true;
        }

		return false;
	}

    bool TerrainEntities::SetTerrainProperty(const PropertyInitializer& prop)
    {
        if (prop._prop == Property_CellsDir) {

            ::Assets::ResChar buffer[MaxPath];
            ucs2_2_utf8((const ucs2*)prop._src, prop._arrayCount, (utf8*)buffer, dimof(buffer));
            _cellsDir = buffer;
            
            ReloadTerrain();

            return true;

        } else if (prop._prop == Property_UberSurfaceDir) {

            ::Assets::ResChar buffer[MaxPath];
            ucs2_2_utf8((const ucs2*)prop._src, prop._arrayCount, (utf8*)buffer, dimof(buffer));
            _uberSurfaceDir = buffer;

            if (!_uberSurfaceDir.empty())
                _terrainManager->LoadUberSurface(buffer);
            return true;

        } else if (prop._prop == Property_Offset) {
            _terrainManager->SetWorldSpaceOrigin(*(const Float3*)prop._src);
            return true;
        }

        return false;
    }

	bool TerrainEntities::GetProperty(
		const Identifier& id, PropertyId prop,
		void* dest, unsigned* destSize) const
	{
		assert(0);		
		return false;
	}

    bool TerrainEntities::SetParent(const Identifier& child, const Identifier& parent, int insertionPosition)
    {
        return false;
    }

	ObjectTypeId TerrainEntities::GetTypeId(const char name[]) const
	{
		if (!XlCompareString(name, "Terrain")) return ObjectType_Terrain;
		return 0;
	}

	DocumentTypeId TerrainEntities::GetDocumentTypeId(const char name[]) const
	{
		return 0;
	}

	PropertyId TerrainEntities::GetPropertyId(ObjectTypeId type, const char name[]) const
	{
        if (type == ObjectType_Terrain) {
		    if (!XlCompareString(name, "CellsDir"))         return Property_CellsDir;
            if (!XlCompareString(name, "UberSurfaceDir"))   return Property_UberSurfaceDir;
            if (!XlCompareString(name, "Offset"))           return Property_Offset;
        }

		return 0;
	}

	ChildListId TerrainEntities::GetChildListId(ObjectTypeId type, const char name[]) const
	{
		return 0;
	}

    void TerrainEntities::UnloadTerrain()
    {
        _terrainManager->Reset();
    }

    void TerrainEntities::ReloadTerrain()
    {
        TRY
        {
		    SceneEngine::TerrainConfig cfg(_cellsDir.c_str());
            cfg._textureCfgName = "";

            _terrainManager->Load(cfg, Int2(0, 0), cfg._cellCount, true);
            if (!_uberSurfaceDir.empty())
                _terrainManager->LoadUberSurface(_uberSurfaceDir.c_str());
        } CATCH (...) {
            _terrainManager->Reset();
            _cellsDir = ::Assets::rstring();
        } CATCH_END
    }

	TerrainEntities::TerrainEntities(
        std::shared_ptr<SceneEngine::TerrainManager> terrainManager) 
    : _terrainManager(std::move(terrainManager))
    {
    }

	TerrainEntities::~TerrainEntities() {}

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    static void UpdateTerrainBaseTexture(
        const RetainedEntities& sys,
        const RetainedEntity& obj)
    {
        auto& divAsset = *Assets::GetDivergentAsset<SceneEngine::TerrainMaterialScaffold>();
        auto trans = divAsset.Transaction_Begin("UpdateTextureProperties");
        if (trans) {
            static auto diffusedims = ParameterBox::MakeParameterNameHash("diffusedims");
            static auto normaldims = ParameterBox::MakeParameterNameHash("normaldims");
            static auto paramdims = ParameterBox::MakeParameterNameHash("paramdims");
            static auto texture0 = ParameterBox::MakeParameterNameHash("texture0");
            static auto texture1 = ParameterBox::MakeParameterNameHash("texture1");
            static auto texture2 = ParameterBox::MakeParameterNameHash("texture2");
            static auto mapping0 = ParameterBox::MakeParameterNameHash("mapping0");
            static auto mapping1 = ParameterBox::MakeParameterNameHash("mapping1");
            static auto mapping2 = ParameterBox::MakeParameterNameHash("mapping2");
            static auto endheight = ParameterBox::MakeParameterNameHash("endheight");

            auto& asset = trans->GetAsset();
            asset._diffuseDims = obj._properties.GetParameter<UInt2>(diffusedims, UInt2(512, 512));
            asset._normalDims = obj._properties.GetParameter<UInt2>(normaldims, UInt2(512, 512));
            asset._paramDims = obj._properties.GetParameter<UInt2>(paramdims, UInt2(512, 512));
            asset._strata.clear();
            for (auto c=obj._children.begin(); c!=obj._children.end(); ++c) {
                auto* strataObj = sys.GetEntity(obj._doc, *c);
                if (!strataObj) continue;

                SceneEngine::TerrainMaterialScaffold::Strata newStrata;
                newStrata._texture[0] = strataObj->_properties.GetString<::Assets::ResChar>(texture0);
                newStrata._texture[1] = strataObj->_properties.GetString<::Assets::ResChar>(texture1);
                newStrata._texture[2] = strataObj->_properties.GetString<::Assets::ResChar>(texture2);
                newStrata._mappingConstant[0] = strataObj->_properties.GetParameter<float>(mapping0, 10.f);
                newStrata._mappingConstant[1] = strataObj->_properties.GetParameter<float>(mapping1, 10.f);
                newStrata._mappingConstant[2] = strataObj->_properties.GetParameter<float>(mapping2, 10.f);
                newStrata._endHeight = strataObj->_properties.GetParameter<float>(endheight, 1000.f);
                asset._strata.push_back(newStrata);
            }

            trans->Commit();
        }
    }

    void RegisterTerrainFlexObjects(RetainedEntities& flexSys)
    {
        flexSys.RegisterCallback(
            flexSys.GetTypeId((const utf8*)"TerrainBaseTexture"),
            [](const RetainedEntities& flexSys, const Identifier& obj)
            {
                auto* object = flexSys.GetEntity(obj);
                if (object)
                    UpdateTerrainBaseTexture(flexSys, *object);
            }
        );
    }

    void ReloadTerrainFlexObjects(RetainedEntities& flexSys)
    {
        auto objs = flexSys.FindEntitiesOfType(flexSys.GetTypeId((const utf8*)"TerrainBaseTexture"));
        for (auto i=objs.cbegin(); i!=objs.cend(); ++i)
            UpdateTerrainBaseTexture(flexSys, **i);
    }
}
