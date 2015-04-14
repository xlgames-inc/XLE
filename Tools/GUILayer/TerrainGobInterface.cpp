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
#include "../../Assets/Assets.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ParameterBox.h"
#include "../../Math/Transformations.h"

namespace GUILayer
{
    void TerrainGob::SetBaseDir(const Assets::ResChar dir[])
    {
        _terrainManager.reset();

        ::Assets::ResChar buffer[MaxPath];
        ucs2_2_utf8((const ucs2*)dir, XlStringLen((const ucs2*)dir), (utf8*)buffer, dimof(buffer));

		SceneEngine::TerrainConfig cfg(buffer);
        cfg._textureCfgName = "DynTerrainTextureCfg";
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
        return *Assets::GetDivergentAsset<SceneEngine::TerrainMaterialScaffold>("DynTerrainTextureCfg");
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

	ObjectId FlexObjectType::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId typeId) const
    {
        return _nextObjectId++;
    }

	bool FlexObjectType::CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, const char initializer[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;

        for (auto i=_objects.cbegin(); i!=_objects.cend(); ++i)
            if (i->_doc == doc && i->_id == obj) return false;

        Object newObject;
        newObject._doc = doc;
        newObject._id = obj;
        newObject._type = typeId;
        newObject._parent = 0;
        _objects.push_back(std::move(newObject));

        InvokeOnChange(*type, _objects[_objects.size()-1]);
        return true;
    }

	bool FlexObjectType::DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const
    {
        for (auto i=_objects.cbegin(); i!=_objects.cend(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                assert(i->_type == objType);
                assert(i->_doc == doc);
                _objects.erase(i);
                return true;
            }
        return false;
    }

	bool FlexObjectType::SetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, 
        const void* src, unsigned elementType, unsigned arrayCount) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;
        if (!src) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                i->_properties.SetParameter(
                    propertyName.c_str(), src, 
                    ImpliedTyping::TypeDesc((ImpliedTyping::TypeCat)elementType, (uint16)arrayCount));

                InvokeOnChange(*type, *i);
                return true;
            }

        return false;
    }

	bool FlexObjectType::GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId typeId, PropertyId prop, void* dest, size_t* destSize) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        if (prop == 0 || prop > type->_properties.size()) return false;

        const auto& propertyName = type->_properties[prop-1];

        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_doc == doc && i->_id == obj) {
                auto res = i->_properties.GetParameter<unsigned>(propertyName.c_str());
                if (res.first) {
                    *(unsigned*)dest = res.second;
                }
                return true;
            }

        return false;
    }

    bool FlexObjectType::SetParent(
        EditorScene& scene, DocumentId doc, 
        ObjectId child, ObjectTypeId childTypeId, 
        ObjectId parent, ObjectTypeId parentTypeId, int insertionPosition) const
    {
        auto childType = GetObjectType(childTypeId);
        if (!childType) return false;

        auto* childObj = GetObjectInt(doc, child);
        if (!childObj || childObj->_type != childTypeId)
            return false;

        if (childObj->_parent != 0) {
            auto* oldParent = GetObjectInt(doc, childObj->_parent);
            if (oldParent) {
                auto i = std::find(oldParent->_children.begin(), oldParent->_children.end(), child);
                oldParent->_children.erase(i);
            }
            childObj->_parent = 0;
        }

        if (!parent) return true;

        auto* parentObj = GetObjectInt(doc, parent);
        if (!parentObj || parentObj->_type != parentTypeId) return false;

        auto parentType = GetObjectType(parentTypeId);
        if (!parentType) return false;

        if (insertionPosition < 0 || insertionPosition >= (int)parentObj->_children.size()) {
            parentObj->_children.push_back(child);
        } else {
            parentObj->_children.insert(
                parentObj->_children.begin() + insertionPosition,
                child);
        }
        childObj->_parent = parentObj->_id;
        return true;
    }

	ObjectTypeId FlexObjectType::GetTypeId(const char name[]) const
    {
        for (auto i=_registeredObjectTypes.cbegin(); i!=_registeredObjectTypes.cend(); ++i)
            if (!XlCompareStringI(i->second._name.c_str(), name))
                return i->first;
        
        _registeredObjectTypes.push_back(
            std::make_pair(_nextObjectTypeId, RegisteredObjectType(name)));
        return _nextObjectTypeId++;
    }

	PropertyId FlexObjectType::GetPropertyId(ObjectTypeId typeId, const char name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_properties.cbegin(); i!=type->_properties.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)(1+std::distance(type->_properties.cbegin(), i));
        
        type->_properties.push_back(name);
        return (PropertyId)type->_properties.size();
    }

	ChildListId FlexObjectType::GetChildListId(ObjectTypeId typeId, const char name[]) const
    {
        auto type = GetObjectType(typeId);
        if (!type) return 0;

        for (auto i=type->_childLists.cbegin(); i!=type->_childLists.cend(); ++i)
            if (!XlCompareStringI(i->c_str(), name)) 
                return (PropertyId)std::distance(type->_childLists.cbegin(), i);
        
        type->_childLists.push_back(name);
        return (PropertyId)(type->_childLists.size()-1);
    }

    auto FlexObjectType::GetObjectType(ObjectTypeId id) const -> RegisteredObjectType*
    {
        for (auto i=_registeredObjectTypes.begin(); i!=_registeredObjectTypes.end(); ++i)
            if (i->first == id) return &i->second;
        return nullptr;
    }

    bool FlexObjectType::RegisterCallback(ObjectTypeId typeId, OnChangeDelegate onChange)
    {
        auto type = GetObjectType(typeId);
        if (!type) return false;
        type->_onChange.push_back(std::move(onChange));
        return true;
    }

    void FlexObjectType::InvokeOnChange(RegisteredObjectType& type, Object& obj) const
    {
        for (auto i=type._onChange.begin(); i!=type._onChange.end(); ++i) {
            (*i)(*this, obj._doc, obj._id, obj._type);
        }

        if (obj._parent != 0)
            for (auto i=_objects.begin(); i!=_objects.end(); ++i)
                if (i->_id == obj._parent && i->_doc == obj._doc) {
                    auto type = GetObjectType(i->_type);
                    if (type) 
                        InvokeOnChange(*type, *i);
                }
    }

    auto FlexObjectType::GetObject(DocumentId doc, ObjectId obj) const -> const Object*
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    auto FlexObjectType::GetObjectInt(DocumentId doc, ObjectId obj) const -> Object* 
    {
        for (auto i=_objects.begin(); i!=_objects.end(); ++i)
            if (i->_id == obj && i->_doc == doc) {
                return AsPointer(i);
            }
        return nullptr;
    }

    DocumentId FlexObjectType::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
    {
        return 0;
    }

	bool FlexObjectType::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
    {
        return false;
    }

	DocumentTypeId FlexObjectType::GetDocumentTypeId(const char name[]) const
    {
        return 0;
    }

	FlexObjectType::FlexObjectType()
    {
        _nextObjectTypeId = 1;
    }

	FlexObjectType::~FlexObjectType()
    {}

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


