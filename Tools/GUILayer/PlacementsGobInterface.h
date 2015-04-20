// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "ManipulatorUtils.h"

namespace GUILayer { namespace EditorDynamicInterface
{
    class PlacementObjectType : public IObjectType
    {
    public:
        DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
        bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

        ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
        bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const PropertyInitializer initializers[], size_t initializerCount) const;
        bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
        bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const PropertyInitializer initializers[], size_t initializerCount) const;
        bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, size_t* destSize) const;
        bool SetParent(EditorScene& scene, DocumentId doc, ObjectId child, ObjectTypeId childType, ObjectId parent, ObjectTypeId parentType, int insertionPosition) const;

        ObjectTypeId GetTypeId(const char name[]) const;
        DocumentTypeId GetDocumentTypeId(const char name[]) const;
        PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
        ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

        PlacementObjectType();
        ~PlacementObjectType();

        static const DocumentTypeId DocumentType_Placements = 1; //(DocumentTypeId)ConstHash64<'plac', 'emen', 'tsdo', 'c'>::Value;
        static const ObjectTypeId ObjectType_Placement = 1; // (ObjectTypeId)ConstHash64<'plac', 'emen', 't'>::Value;
        static const PropertyId Property_Transform = 100;
        static const PropertyId Property_Visible = 101;
        static const PropertyId Property_Model = 102;
        static const PropertyId Property_Material = 103;
        static const PropertyId Property_Bounds = 104;
        static const PropertyId Property_LocalBounds = 105;
    };
}}

namespace SceneEngine { class PlacementsEditor; }
namespace ToolsRig { class IPlacementManipulatorSettings; }

namespace GUILayer
{
    class PlacementManipulatorsPimpl
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
		std::vector<RegisteredManipulator> _manipulators;
    };

    ref class PlacementManipulators : public IManipulatorSet
    {
    public:
        virtual clix::shared_ptr<ToolsRig::IManipulator> GetManipulator(System::String^ name) override;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() override;

        PlacementManipulators(
            ToolsRig::IPlacementManipulatorSettings* context,
            std::shared_ptr<SceneEngine::PlacementsEditor> editor);
        ~PlacementManipulators();
    protected:
        clix::auto_ptr<PlacementManipulatorsPimpl> _pimpl;
    };

    public ref class IPlacementManipulatorSettingsLayer abstract
    {
    public:
        virtual String^ GetSelectedModel() = 0;
        virtual void EnableSelectedModelDisplay(bool newState) = 0;
        virtual void SelectModel(String^ newModelName) = 0;
        virtual void SwitchToMode(unsigned newMode) = 0;

        ToolsRig::IPlacementManipulatorSettings* GetNative();
        
        IPlacementManipulatorSettingsLayer();
        virtual ~IPlacementManipulatorSettingsLayer();
    private:
        clix::auto_ptr<ToolsRig::IPlacementManipulatorSettings> _native;
    };
}
