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
#include "../../Utility/StringFormat.h"

namespace EntityInterface
{
    static const ObjectTypeId ObjectType_Terrain = 1;
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
        if (prop._prop == Property_UberSurfaceDir) {

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
            if (!_uberSurfaceDir.empty())
                _terrainManager->LoadUberSurface(_uberSurfaceDir.c_str());
        } CATCH (...) {
            _terrainManager->Reset();
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
        const RetainedEntity& obj,
        SceneEngine::TerrainManager& terrainMan)
    {
        using namespace SceneEngine;

        static auto diffusedims = ParameterBox::MakeParameterNameHash("diffusedims");
        static auto normaldims = ParameterBox::MakeParameterNameHash("normaldims");
        static auto paramdims = ParameterBox::MakeParameterNameHash("paramdims");

        TerrainMaterialConfig asset;
        asset._diffuseDims = obj._properties.GetParameter<UInt2>(diffusedims, UInt2(512, 512));
        asset._normalDims = obj._properties.GetParameter<UInt2>(normaldims, UInt2(512, 512));
        asset._paramDims = obj._properties.GetParameter<UInt2>(paramdims, UInt2(512, 512));

        static auto materialId = ParameterBox::MakeParameterNameHash("MaterialId");
        static auto texture0 = ParameterBox::MakeParameterNameHash("texture0");
        static auto texture1 = ParameterBox::MakeParameterNameHash("texture1");
        static auto texture2 = ParameterBox::MakeParameterNameHash("texture2");
        static auto mapping0 = ParameterBox::MakeParameterNameHash("mapping0");
        static auto mapping1 = ParameterBox::MakeParameterNameHash("mapping1");
        static auto mapping2 = ParameterBox::MakeParameterNameHash("mapping2");
        static auto endheight = ParameterBox::MakeParameterNameHash("endheight");

            // rebuild all of the material binding information as well
        {
            asset._strataMaterials.clear();
            auto matType = sys.GetTypeId((const utf8*)"TerrainStrataMaterial");
            for (auto c=obj._children.cbegin(); c!=obj._children.end(); ++c) {
                auto* mat = sys.GetEntity(obj._doc, *c);
                if (!mat || mat->_type != matType) continue;

                TerrainMaterialConfig::StrataMaterial nativeMat;
                nativeMat._id = mat->_properties.GetParameter<unsigned>(materialId, 0);

                for (auto c=mat->_children.begin(); c!=mat->_children.end(); ++c) {
                    auto* strataObj = sys.GetEntity(mat->_doc, *c);
                    if (!strataObj) continue;

                    TerrainMaterialConfig::StrataMaterial::Strata newStrata;
                    newStrata._texture[0] = strataObj->_properties.GetString<::Assets::ResChar>(texture0);
                    newStrata._texture[1] = strataObj->_properties.GetString<::Assets::ResChar>(texture1);
                    newStrata._texture[2] = strataObj->_properties.GetString<::Assets::ResChar>(texture2);
                    newStrata._mappingConstant[0] = strataObj->_properties.GetParameter<float>(mapping0, 10.f);
                    newStrata._mappingConstant[1] = strataObj->_properties.GetParameter<float>(mapping1, 10.f);
                    newStrata._mappingConstant[2] = strataObj->_properties.GetParameter<float>(mapping2, 10.f);
                    newStrata._endHeight = strataObj->_properties.GetParameter<float>(endheight, 1000.f);
                    nativeMat._strata.push_back(newStrata);
                }

                asset._strataMaterials.push_back(std::move(nativeMat));
            }
        }

        {
            asset._gradFlagMaterials.clear();
            auto matType = sys.GetTypeId((const utf8*)"TerrainGradFlagMaterial");
            for (auto c=obj._children.cbegin(); c!=obj._children.end(); ++c) {
                auto* mat = sys.GetEntity(obj._doc, *c);
                if (!mat || mat->_type != matType) continue;

                TerrainMaterialConfig::GradFlagMaterial nativeMat;
                nativeMat._id = mat->_properties.GetParameter<unsigned>(materialId, 0);

                for (unsigned c=0; c<dimof(nativeMat._texture); ++c) {
                    auto textureHash = ParameterBox::MakeParameterNameHash(StringMeld<128>() << "Texture" << c);
                    auto mappingHash = ParameterBox::MakeParameterNameHash(StringMeld<128>() << "TextureMapping" << c);
                    nativeMat._texture[c] = mat->_properties.GetString<::Assets::ResChar>(textureHash);
                    nativeMat._mappingConstant[c] = mat->_properties.GetParameter(mappingHash, 1.f);
                }

                asset._gradFlagMaterials.push_back(std::move(nativeMat));
            }
        }

        {
            asset._procTextures.clear();
            auto matType = sys.GetTypeId((const utf8*)"TerrainProcTexture");
            for (auto c=obj._children.cbegin(); c!=obj._children.end(); ++c) {
                auto* mat = sys.GetEntity(obj._doc, *c);
                if (!mat || mat->_type != matType) continue;

                static auto nameHash = ParameterBox::MakeParameterNameHash("Name");
                static auto textureHash0 = ParameterBox::MakeParameterNameHash("Texture0");
                static auto textureHash1 = ParameterBox::MakeParameterNameHash("Texture1");
                static auto hgridHash = ParameterBox::MakeParameterNameHash("HGrid");
                static auto gainHash = ParameterBox::MakeParameterNameHash("Gain");
                    
                TerrainMaterialConfig::ProcTextureSetting procTexture;
                procTexture._name = mat->_properties.GetString<::Assets::ResChar>(nameHash);
                procTexture._texture[0] = mat->_properties.GetString<::Assets::ResChar>(textureHash0);
                procTexture._texture[1] = mat->_properties.GetString<::Assets::ResChar>(textureHash1);
                procTexture._hgrid = mat->_properties.GetParameter(hgridHash, procTexture._hgrid);
                procTexture._gain = mat->_properties.GetParameter(gainHash, procTexture._gain);
                asset._procTextures.push_back(std::move(procTexture));
            }
        }

        terrainMan.LoadMaterial(asset);
    }

    static void ClearTerrainBaseTexture(SceneEngine::TerrainManager& terrainMan)
    {
        terrainMan.LoadMaterial(SceneEngine::TerrainMaterialConfig());
    }

    void RegisterTerrainFlexObjects(
        RetainedEntities& flexSys, 
        std::shared_ptr<SceneEngine::TerrainManager> terrainMan)
    {
        std::weak_ptr<SceneEngine::TerrainManager> weakPtrTerrainMan = terrainMan;
        flexSys.RegisterCallback(
            flexSys.GetTypeId((const utf8*)"TerrainBaseTexture"),
            [weakPtrTerrainMan](const RetainedEntities& flexSys, const Identifier& obj, RetainedEntities::ChangeType changeType)
            {
                auto l = weakPtrTerrainMan.lock();
                if (!l) return;

                if (changeType != RetainedEntities::ChangeType::Delete) {
                    auto* object = flexSys.GetEntity(obj);
                    if (object)
                        UpdateTerrainBaseTexture(flexSys, *object, *l);
                } else {
                    ClearTerrainBaseTexture(*l);
                }
            });
    }

    void ReloadTerrainFlexObjects(RetainedEntities& flexSys, SceneEngine::TerrainManager& terrainMan)
    {
        auto objs = flexSys.FindEntitiesOfType(flexSys.GetTypeId((const utf8*)"TerrainBaseTexture"));
        for (auto i=objs.cbegin(); i!=objs.cend(); ++i)
            UpdateTerrainBaseTexture(flexSys, **i, terrainMan);
    }
}
