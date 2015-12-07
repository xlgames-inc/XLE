// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Xml;
using System.Xml.Schema;
using System.IO;

using Sce.Atf;
using Sce.Atf.Dom;
using Sce.Atf.Adaptation;
using Sce.Atf.Controls.PropertyEditing;

using LevelEditorCore;

namespace LevelEditorXLE
{
    // Theses are functions that are difficult to fit within the 
    // architecture in a better way. These functions are explicitly
    // called from the "LevelEditor" project. But ideally we'd have
    // some better way to attach a library like this, without
    // having to change the LevelEditor project so much.
    public static class Patches
    {
        public static DomNode GetReferenceTarget(DomNode node)
        {
            var placementsCellRef = node.As<Placements.PlacementsCellRef>();
            if (placementsCellRef != null && placementsCellRef.Target != null)
            {
                node = placementsCellRef.Target.As<DomNode>();
            }
            return node;
        }

        public static bool IsReferenceType(DomNodeType nodeType)
        {
            return Schema.placementsCellReferenceType.Type.IsAssignableFrom(nodeType);
        }

        public static void ResolveOnLoad(IAdaptable gameNode)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
            {
                foreach (var cell in placementsFolder.Cells)
                {
                    cell.Resolve();
                }
            }
        }

        public static void Unresolve(IAdaptable gameNode)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
            {
                foreach (var cell in placementsFolder.Cells)
                {
                    cell.Unresolve();
                }
            }

                // Detach all children
                // This will cause the native adapters to destroy the associated native objects.
            foreach (var c in game.DomNode.Children)
                c.RemoveFromParent();
        }

        public static void SaveReferencedDocuments(IAdaptable gameNode, ISchemaLoader schemaLoader)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
            {
                foreach (var cellRef in placementsFolder.Cells)
                {
                    var doc = cellRef.Target;
                    if (doc == null) continue;
                    doc.Save(cellRef.Uri, schemaLoader);
                }
            }
        }

        public static void SelectNameForReferencedDocuments(IAdaptable gameNode)
        {
            var game = gameNode.As<Game.GameExtensions>();
            if (game == null) return;

            var placementsFolder = game.PlacementsFolder;
            if (placementsFolder != null)
                foreach (var cellRef in placementsFolder.Cells)
                    cellRef.SelectNameIfNecessary();
        }

        public static Tuple<string, string> GetSchemaResourceName()
        {
            return new Tuple<string, string>("LevelEditorXLE.Schema", "xleroot.xsd");
        }

        public static void CreateDefaultNodes(DomNode gameRoot)
        {
            var ext = gameRoot.As<Game.GameExtensions>();
            if (ext != null)
            {
                    // Add default environment settings
                var envFolder = ext.EnvSettingsFolder;
                if (envFolder.Settings.Count == 0)
                    envFolder.AddChild(Environment.XLEEnvSettings.Create(envFolder.GetNameForNewChild()));

                    // Add placements folder with an unnamed default placement document
                if (ext.PlacementsFolder == null)
                {
                    var newNode = Placements.PlacementsFolder.CreateStarter();
                    if (newNode != null)
                    {
                        ext.PlacementsFolder = newNode.As<Placements.PlacementsFolder>();
                    }
                }
            }
        }

        private static EmbeddedCollectionEditor SetupEmbeddedCollectionEditor(DomNodeType containedType, string objectName)
        {
            var result = new EmbeddedCollectionEditor();
            result.GetItemInsertersFunc = (context) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list == null) return EmptyArray<EmbeddedCollectionEditor.ItemInserter>.Instance;

                // create ItemInserter for each component type.
                var insertors = new EmbeddedCollectionEditor.ItemInserter[1]
                    {
                        new EmbeddedCollectionEditor.ItemInserter(objectName,
                        delegate
                        {
                            DomNode node = new DomNode(containedType);
                            list.Add(node);
                            return node;
                        })
                    };
                return insertors;
            };

            result.RemoveItemFunc = (context, item) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list != null)
                    list.Remove(item.Cast<DomNode>());
            };

            result.MoveItemFunc = (context, item, delta) =>
            {
                var list = context.GetValue() as IList<DomNode>;
                if (list != null)
                {
                    DomNode node = item.Cast<DomNode>();
                    int index = list.IndexOf(node);
                    int insertIndex = index + delta;
                    if (insertIndex < 0 || insertIndex >= list.Count)
                        return;
                    list.RemoveAt(index);
                    list.Insert(insertIndex, node);
                }
            };

            return result;
        }

        public static void OnSchemaSetLoaded(
            XmlSchemaSet schemaSet, 
            IEnumerable<XmlSchemaTypeCollection> typeCollections)
        {
            foreach (XmlSchemaTypeCollection typeCollection in typeCollections)
            {
                Schema.Initialize(typeCollection);
                Startup.InitializeAdapters();
                break;
            }

            Schema.terrainStrataMaterialType.Type.SetTag(
                new PropertyDescriptorCollection(
                    new System.ComponentModel.PropertyDescriptor[] {
                        new ChildPropertyDescriptor(
                            "Strata".Localize(),
                            Schema.terrainStrataMaterialType.strataChild,
                            null,
                            "List of texturing stratas".Localize(),
                            false,
                            SetupEmbeddedCollectionEditor(Schema.terrainBaseTextureStrataType.Type, "Strata"))
                            }));

            Schema.vegetationSpawnMaterialType.Type.SetTag(
                new PropertyDescriptorCollection(
                    new System.ComponentModel.PropertyDescriptor[] {
                        new ChildPropertyDescriptor(
                            "Object Type".Localize(),
                            Schema.vegetationSpawnMaterialType.objectChild,
                            null,
                            "List of spawned objects".Localize(),
                            false,
                            SetupEmbeddedCollectionEditor(Schema.vegetationSpawnObjectType.Type, "Object Type"))
                            }));

            Schema.triMeshMarkerType.Type.SetTag(
                new PropertyDescriptorCollection(
                    new System.ComponentModel.PropertyDescriptor[] {
                        new ChildPropertyDescriptor(
                            "Vertex".Localize(),
                            Schema.triMeshMarkerType.pointsChild,
                            null,
                            "List of mesh vertices".Localize(),
                            false,
                            SetupEmbeddedCollectionEditor(Schema.markerPointType.Type, "Vertex"))
                            }));
        }

        public static DomNodeType GetGameType() { return Schema.xleGameType.Type; }
    }
}
