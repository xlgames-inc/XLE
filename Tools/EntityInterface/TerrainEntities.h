// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EntityInterface.h"
#include "../../Assets/Assets.h"
#include "../../Math/Vector.h"
#include "../../Utility/UTFUtils.h"
#include <vector>

namespace Tools { class IManipulator; }
namespace SceneEngine { class TerrainManager; class TerrainMaterialScaffold; }

namespace EntityInterface
{
    class TerrainEntities : public IEntityInterface
	{
	public:
		DocumentId CreateDocument(DocumentTypeId docType, const char initializer[]);
		bool DeleteDocument(DocumentId doc, DocumentTypeId docType);

		ObjectId AssignObjectId(DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount);
		bool DeleteObject(const Identifier& id);
		bool SetProperty(const Identifier& id, const PropertyInitializer initializers[], size_t initializerCount);
		bool GetProperty(const Identifier& id, PropertyId prop, void* dest, unsigned* destSize) const;
        bool SetParent(const Identifier& child, const Identifier& parent, int insertionPosition);

		ObjectTypeId GetTypeId(const char name[]) const;
		DocumentTypeId GetDocumentTypeId(const char name[]) const;
		PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
		ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

        void UnloadTerrain();
        void ReloadTerrain();

		TerrainEntities(std::shared_ptr<SceneEngine::TerrainManager> terrainManager);
		~TerrainEntities();

    private:
        bool SetTerrainProperty(const PropertyInitializer& prop);
        std::shared_ptr<SceneEngine::TerrainManager> _terrainManager;
        ::Assets::rstring _uberSurfaceDir;
        ::Assets::rstring _cellsDir;
	};

    class RetainedEntities;
    void RegisterTerrainFlexObjects(RetainedEntities& flexSys);
    void ReloadTerrainFlexObjects(RetainedEntities& flexSys);
}
