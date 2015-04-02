// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "EditorDynamicInterface.h"

namespace GUILayer { namespace EditorDynamicInterface
{
	class TerrainObjectType : public IObjectType
	{
	public:
		DocumentId CreateDocument(EditorScene& scene, DocumentTypeId docType, const char initializer[]) const;
		bool DeleteDocument(EditorScene& scene, DocumentId doc, DocumentTypeId docType) const;

		ObjectId AssignObjectId(EditorScene& scene, DocumentId doc, ObjectTypeId type) const;
		bool CreateObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, const char initializer[]) const;
		bool DeleteObject(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId objType) const;
		bool SetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, const void* src, size_t srcSize) const;
		bool GetProperty(EditorScene& scene, DocumentId doc, ObjectId obj, ObjectTypeId type, PropertyId prop, void* dest, size_t* destSize) const;

		ObjectTypeId GetTypeId(const char name[]) const;
		DocumentTypeId GetDocumentTypeId(const char name[]) const;
		PropertyId GetPropertyId(ObjectTypeId type, const char name[]) const;
		ChildListId GetChildListId(ObjectTypeId type, const char name[]) const;

		TerrainObjectType();
		~TerrainObjectType();

		static const ObjectTypeId ObjectType_Terrain = 1; // (ObjectTypeId)ConstHash64<'plac', 'emen', 't'>::Value;
		static const PropertyId Property_BaseDir = 200;
	};
}}
