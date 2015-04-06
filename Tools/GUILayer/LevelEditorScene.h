// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"
#include "AutoToShared.h"
#include "../../Tools/ToolsRig/IManipulator.h"
#include "../../Assets/Assets.h"    // just for ResChar
#include "../../Math/Vector.h"
#include <memory>

namespace SceneEngine { class PlacementsManager; class PlacementsEditor; class ISceneParser; class IntersectionTestScene; class TerrainManager; }
namespace Tools { class IManipulator; }

namespace GUILayer
{
    ref class VisCameraSettings;
    ref class IntersectionTestContextWrapper;
	ref class IntersectionTestSceneWrapper;

    class TerrainGob
    {
    public:
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;

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

        void SetBaseDir(const Assets::ResChar dir[]);
        void SetOffset(const Float3& offset);

        TerrainGob();
        ~TerrainGob();

    protected:
        Float3 _terrainOffset;
    };

    class EditorScene
    {
    public:
        std::shared_ptr<SceneEngine::PlacementsManager> _placementsManager;
        std::shared_ptr<SceneEngine::PlacementsEditor> _placementsEditor;
        std::unique_ptr<TerrainGob> _terrainGob;

        EditorScene();
		~EditorScene();
    };

    namespace EditorDynamicInterface { class RegisteredTypes; }

    ref class IOverlaySystem;

    public ref class IManipulatorSet abstract
    {
    public:
        virtual ToolsRig::IManipulator* GetManipulator(System::String^ name) = 0;
		virtual System::Collections::Generic::IEnumerable<System::String^>^ GetManipulatorNames() = 0;
        virtual ~IManipulatorSet();
    };

    public ref class EditorSceneManager
    {
    public:
            //// //// ////   U T I L S   //// //// ////
        IManipulatorSet^ GetTerrainManipulators();
        IOverlaySystem^ CreateOverlaySystem(VisCameraSettings^ camera);
		IntersectionTestSceneWrapper^ GetIntersectionScene();

            //// //// ////   G O B   I N T E R F A C E   //// //// ////
        using DocumentTypeId = EditorDynamicInterface::DocumentTypeId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using DocumentId = EditorDynamicInterface::DocumentId;
        using ObjectId = EditorDynamicInterface::ObjectId;
        using ObjectTypeId = EditorDynamicInterface::ObjectTypeId;
        using PropertyId = EditorDynamicInterface::PropertyId;
        using ChildListId = EditorDynamicInterface::ChildListId;

        DocumentId CreateDocument(DocumentTypeId docType);
        bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

        ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type);
        bool CreateObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool DeleteObject(DocumentId doc, ObjectId obj, ObjectTypeId objType);
        bool SetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, const void* src, size_t srcSize);
        bool GetProperty(DocumentId doc, ObjectId obj, ObjectTypeId objType, PropertyId prop, void* dest, size_t* destSize);

        ObjectTypeId GetTypeId(System::String^ name);
        DocumentTypeId GetDocumentTypeId(System::String^ name);
        PropertyId GetPropertyId(ObjectTypeId type, System::String^ name);
        ChildListId GetChildListId(ObjectTypeId type, System::String^ name);

            //// //// ////   C O N S T R U C T O R S   //// //// ////
        EditorSceneManager();
        ~EditorSceneManager();
        !EditorSceneManager();
    protected:
        AutoToShared<EditorScene> _scene;
        AutoToShared<EditorDynamicInterface::RegisteredTypes> _dynInterface;
        IManipulatorSet^ _terrainManipulators;
    };
}


