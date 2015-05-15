// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PlacementsGobInterface.h"
#include "LevelEditorScene.h"
#include "ExportedNativeTypes.h"
#include "MarshalString.h"
#include "../ToolsRig/PlacementsManipulators.h"
#include "../ToolsRig/IManipulator.h"
#include "../../SceneEngine/PlacementsManager.h"
#include "../../Utility/StringFormat.h"
#include "../../Utility/ParameterBox.h"
#include "../../Utility/UTFUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Math/Transformations.h"

namespace GUILayer { namespace EditorDynamicInterface
{

    DocumentId PlacementObjectType::CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const
    {
        if (docType != DocumentType_Placements) { assert(0); return 0; }

        StringMeld<MaxPath, ::Assets::ResChar> meld;
        meld << "[dyn] " << initializer;

        return (DocumentId)scene._placementsEditor->CreateCell(
            *scene._placementsManager,
            meld,  Float2(-1000.f, -1000.f), Float2( 1000.f,  1000.f));
    }

    bool PlacementObjectType::DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const
    {
        if (docType != DocumentType_Placements) { assert(0); return false; }
        scene._placementsEditor->RemoveCell(*scene._placementsManager, doc);
        return true;
    }

    ObjectId PlacementObjectType::AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const
    {
        if (type != ObjectType_Placement) { assert(0); return 0; }
        return scene._placementsEditor->GenerateObjectGUID();
    }

    static bool SetObjProperty(
        SceneEngine::PlacementsEditor::ObjTransDef& obj, 
        const IObjectType::PropertyInitializer& prop)
    {
        if (prop._prop == PlacementObjectType::Property_Transform) {
                // note -- putting in a transpose here, because the level editor matrix
                //          math uses a transposed form
            if (prop._elementType == (unsigned)ImpliedTyping::TypeCat::Float && prop._arrayCount >= 16) {
                obj._localToWorld = AsFloat3x4(Transpose(*(const Float4x4*)prop._src));
                return true;
            }

        } else if (prop._prop == PlacementObjectType::Property_Model || prop._prop == PlacementObjectType::Property_Material) {
            Assets::ResChar buffer[MaxPath];
            ucs2_2_utf8(
                (const ucs2*)prop._src, prop._arrayCount,
                (utf8*)buffer, dimof(buffer));

            if (prop._prop == PlacementObjectType::Property_Model) {
                obj._model = buffer;
            } else {
                obj._material = buffer;
            }
            return true;
        }
        return false;
    }

    bool PlacementObjectType::CreateObject(
        EditorScene& scene, DocumentId doc, 
        ObjectId obj, ObjectTypeId type, 
        const PropertyInitializer initializers[], size_t initializerCount) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }

        SceneEngine::PlacementsEditor::ObjTransDef newObj;
        newObj._localToWorld = Identity<decltype(newObj._localToWorld)>();
        newObj._model = "game/model/nature/bushtree/BushE";
        newObj._material = "game/model/nature/bushtree/BushE";

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(nullptr, nullptr);
        if (transaction->Create(guid, newObj)) {

            if (initializerCount) {
                auto originalObject = transaction->GetObject(0);

                bool result = false;
                for (size_t c=0; c<initializerCount; ++c) 
                    result |= SetObjProperty(originalObject, initializers[c]);

                if (result)
                    transaction->SetObject(0, originalObject);
            }

            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementObjectType::DeleteObject(
        EditorScene& scene, DocumentId doc, 
        ObjectId obj, ObjectTypeId type) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            transaction->Delete(0);
            transaction->Commit();
            return true;
        }

        return false;
    }

    bool PlacementObjectType::SetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, 
        ObjectTypeId type, 
        const PropertyInitializer initializers[], size_t initializerCount) const
    {
            // find the object, and set the given property (as per the new value specified in the string)
            //  We need to create a transaction, make the change and then commit it back.
            //  If the transaction returns no results, then we must have got a bad object or document id.
        if (type != ObjectType_Placement) { assert(0); return false; }

            // note --  This object search is quite slow! We might need a better way to
            //          record a handle to the object. Perhaps the "ObjectId" should not
            //          match the actual placements guid. Some short-cut will probably be
            //          necessary given that we could get there several thousand times during
            //          startup for an average scene.
        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            auto originalObject = transaction->GetObject(0);

            bool result = false;
            for (size_t c=0; c<initializerCount; ++c) {
                result |= SetObjProperty(originalObject, initializers[c]);
            }

            if (result) {
                transaction->SetObject(0, originalObject);
                transaction->Commit();
                return true;
            }
        }

        return false;
    }

    bool PlacementObjectType::GetProperty(
        EditorScene& scene, DocumentId doc, ObjectId obj, 
        ObjectTypeId type, PropertyId prop, 
        void* dest, unsigned* destSize) const
    {
        if (type != ObjectType_Placement) { assert(0); return false; }
        if (prop != Property_Transform && prop != Property_Visible
            && prop != Property_Bounds && prop != Property_LocalBounds) { assert(0); return false; }
        assert(destSize);

        typedef std::pair<Float3, Float3> BoundingBox;

        auto guid = SceneEngine::PlacementGUID(doc, obj);
        auto transaction = scene._placementsEditor->Transaction_Begin(
            &guid, &guid+1, 
            SceneEngine::PlacementsEditor::TransactionFlags::IgnoreIdTop32Bits);
        if (transaction->GetObjectCount()==1) {
            if (prop == Property_Transform) {
                if (*destSize >= sizeof(Float4x4)) {
                    auto originalObject = transaction->GetObject(0);
                        // note -- putting in a transpose here, because the level editor matrix
                        //          math uses a transposed form
                    *(Float4x4*)dest = Transpose(AsFloat4x4(originalObject._localToWorld));
                    return true;
                }
                *destSize = sizeof(Float4x4);
            } else if (prop == Property_Bounds) {
                if (*destSize >= sizeof(BoundingBox)) {
                    *(BoundingBox*)dest = transaction->GetWorldBoundingBox(0);
                    return true;
                }
                *destSize = sizeof(BoundingBox);
            } else if (prop == Property_LocalBounds) {
                if (*destSize >= sizeof(BoundingBox)) {
                    *(BoundingBox*)dest = transaction->GetLocalBoundingBox(0);
                    return true;
                }
                *destSize = sizeof(BoundingBox);
            }
        }

        return false;
    }

    bool PlacementObjectType::SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const
    {
        return false;
    }

    ObjectTypeId PlacementObjectType::GetTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementObject")) return ObjectType_Placement;
        return 0;
    }

    DocumentTypeId PlacementObjectType::GetDocumentTypeId(const char name[]) const
    {
        if (!XlCompareString(name, "PlacementsDocument")) return DocumentType_Placements;
        return 0;
    }

    PropertyId PlacementObjectType::GetPropertyId(ObjectTypeId type, const char name[]) const
    {
        if (!XlCompareString(name, "transform"))    return Property_Transform;
        if (!XlCompareString(name, "visible"))      return Property_Visible;
        if (!XlCompareString(name, "model"))        return Property_Model;
        if (!XlCompareString(name, "material"))     return Property_Material;
        if (!XlCompareString(name, "Bounds"))       return Property_Bounds;
        if (!XlCompareString(name, "LocalBounds"))  return Property_LocalBounds;
        return 0;
    }

    ChildListId PlacementObjectType::GetChildListId(ObjectTypeId type, const char name[]) const
    {
        return 0;
    }

    PlacementObjectType::PlacementObjectType() {}
    PlacementObjectType::~PlacementObjectType() {}

}}

namespace GUILayer
{
    PlacementManipulatorsPimpl::RegisteredManipulator::~RegisteredManipulator() {}

	clix::shared_ptr<ToolsRig::IManipulator> PlacementManipulators::GetManipulator(System::String^ name)
	{
		auto nativeName = clix::marshalString<clix::E_UTF8>(name);
		for (auto i : _pimpl->_manipulators)
			if (i._name == nativeName) return clix::shared_ptr<ToolsRig::IManipulator>(i._manipulator);
		return clix::shared_ptr<ToolsRig::IManipulator>();
	}

	System::Collections::Generic::IEnumerable<System::String^>^ PlacementManipulators::GetManipulatorNames()
	{
		auto result = gcnew System::Collections::Generic::List<System::String^>();
		for (auto i : _pimpl->_manipulators)
			result->Add(clix::marshalString<clix::E_UTF8>(i._name));
		return result;
	}

    PlacementManipulators::PlacementManipulators(
        ToolsRig::IPlacementManipulatorSettings* context,
        std::shared_ptr<SceneEngine::PlacementsEditor> editor)
    {
        _pimpl.reset(new PlacementManipulatorsPimpl);

        auto manip = ToolsRig::CreatePlacementManipulators(context, editor);
        for (auto& t : manip) {
            _pimpl->_manipulators.push_back(
                PlacementManipulatorsPimpl::RegisteredManipulator(t->GetName(), std::move(t)));
        }
    }

    PlacementManipulators::~PlacementManipulators() 
    {
        _pimpl.reset();
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    namespace Internal
    {
        class PlacementManipulatorSettingsAdapter : public ToolsRig::IPlacementManipulatorSettings
        {
        public:
            virtual std::string GetSelectedModel() const;
            virtual void EnableSelectedModelDisplay(bool newState);
            virtual void SelectModel(const char newModelName[]);
            virtual void SwitchToMode(Mode::Enum newMode);

            PlacementManipulatorSettingsAdapter(IPlacementManipulatorSettingsLayer^ managed);
            virtual ~PlacementManipulatorSettingsAdapter();
        private:
            gcroot<IPlacementManipulatorSettingsLayer^> _managed;
        };

        std::string PlacementManipulatorSettingsAdapter::GetSelectedModel() const
        {
            return clix::marshalString<clix::E_UTF8>(_managed->GetSelectedModel());
        }
        void PlacementManipulatorSettingsAdapter::EnableSelectedModelDisplay(bool newState)
        {
            return _managed->EnableSelectedModelDisplay(newState);
        }
        void PlacementManipulatorSettingsAdapter::SelectModel(const char newModelName[])
        {
            _managed->SelectModel(
                clix::marshalString<clix::E_UTF8>(newModelName));
        }
        void PlacementManipulatorSettingsAdapter::SwitchToMode(Mode::Enum newMode)
        {
            _managed->SwitchToMode((unsigned)newMode);
        }

        PlacementManipulatorSettingsAdapter::PlacementManipulatorSettingsAdapter(
            IPlacementManipulatorSettingsLayer^ managed)
        {
            _managed = managed;
        }

        PlacementManipulatorSettingsAdapter::~PlacementManipulatorSettingsAdapter()
        {}
    }

    ToolsRig::IPlacementManipulatorSettings* IPlacementManipulatorSettingsLayer::GetNative()
    {
        return _native.get();
    }
        
    IPlacementManipulatorSettingsLayer::IPlacementManipulatorSettingsLayer()
    {
        _native.reset(new Internal::PlacementManipulatorSettingsAdapter(this));
    }

    IPlacementManipulatorSettingsLayer::~IPlacementManipulatorSettingsLayer()
    {
        _native.get();
    }

}

