// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "TerrainGobInterface.h"
#include "LevelEditorScene.h"
#include "FlexGobInterface.h"
#include "ExportedNativeTypes.h"
#include "MarshalString.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/TerrainMaterial.h"
#include "../../SceneEngine/SceneEngineUtils.h"
#include "../../SceneEngine/TerrainFormat.h"
#include "../../Tools/ToolsRig/TerrainManipulators.h"
#include "../../Tools/ToolsRig/IManipulator.h"

namespace GUILayer
{

///////////////////////////////////////////////////////////////////////////////////////////////////

    void TerrainGob::SetBaseDir(const ucs2 dir[], unsigned length)
    {
        _terrainManager.reset();

        ::Assets::ResChar buffer[MaxPath];
        ucs2_2_utf8(dir, length, (utf8*)buffer, dimof(buffer));

		SceneEngine::TerrainConfig cfg(buffer);
        cfg._textureCfgName = "";
		_terrainManager = std::make_shared<SceneEngine::TerrainManager>(
			cfg, std::make_unique<SceneEngine::TerrainFormat>(),
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////

    static void UpdateTerrainBaseTexture(
        const EditorDynamicInterface::FlexObjectType& sys,
        const EditorDynamicInterface::FlexObjectType::Object& obj)
    {
        using namespace EditorDynamicInterface;
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
                auto* strataObj = sys.GetObject(obj._doc, *c);
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

    static void TerrainBaseTextureCallback(
        const EditorDynamicInterface::FlexObjectType& flexSys, 
        EditorDynamicInterface::DocumentId doc, EditorDynamicInterface::ObjectId obj, EditorDynamicInterface::ObjectTypeId type)
    {
        auto* object = flexSys.GetObject(doc, obj);
        if (object) {
            UpdateTerrainBaseTexture(flexSys, *object);
        }
    }

    namespace Internal
    {
        void RegisterTerrainFlexObjects(EditorDynamicInterface::FlexObjectType& flexSys)
        {
            flexSys.RegisterCallback(
                flexSys.GetTypeId("TerrainBaseTexture"),
                &TerrainBaseTextureCallback);
        }
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
		const PropertyInitializer initializers[], size_t initializerCount) const
	{
		if (type == ObjectType_Terrain) {
		    scene._terrainGob = std::make_unique<TerrainGob>();
            for (size_t c=0; c<initializerCount; ++c)
                SetTerrainProperty(scene, initializers[c]);
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
		ObjectTypeId type,
		const PropertyInitializer initializers[], size_t initializerCount) const
	{
		if (type == ObjectType_Terrain) {
            for (size_t c=0; c<initializerCount; ++c)
                SetTerrainProperty(scene, initializers[c]);
            return true;
        }

		return false;
	}

    bool TerrainObjectType::SetTerrainProperty(EditorScene& scene, const PropertyInitializer& prop) const
    {
        if (!scene._terrainGob) {
            assert(0);
            return false;
        }
		
        if (prop._prop == Property_BaseDir) {
            scene._terrainGob->SetBaseDir((const ucs2*)prop._src, prop._arrayCount);
            return true;
        } else if (prop._prop == Property_Offset) {
            scene._terrainGob->SetOffset(*(const Float3*)prop._src);
            return true;
        }

        return false;
    }

	bool TerrainObjectType::GetProperty(
		EditorScene& scene, DocumentId doc, ObjectId obj,
		ObjectTypeId type, PropertyId prop,
		void* dest, unsigned* destSize) const
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

		return 0;
	}

	ChildListId TerrainObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
	{
		return 0;
	}

	TerrainObjectType::TerrainObjectType() {}
	TerrainObjectType::~TerrainObjectType() {}

}}


