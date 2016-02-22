// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;
using LevelEditorXLE.Extensions;

namespace LevelEditorXLE.Terrain
{
    class VegetationSpawnConfigGob : DomNodeAdapter, IHierarchical, IListable, IExportable, ICommandClient, IContextMenuCommandProvider
    {
        public bool CanAddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode == null) return false;

            return domNode.Type.Lineage.FirstOrDefault(t => t == Schema.vegetationSpawnMaterialType.Type) != null;
        }

        public bool AddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode != null && domNode.Type.Lineage.FirstOrDefault(t => t == Schema.vegetationSpawnMaterialType.Type) != null)
            {
                GetChildList<DomNode>(Schema.vegetationSpawnConfigType.materialChild).Add(domNode);
                return true;
            }

            return false;
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Vegetation Spawn";
        }

        public IEnumerable<int> GetMaterialIds()
        {
            var children = GetChildList<DomNode>(Schema.vegetationSpawnConfigType.materialChild);
            foreach (var i in children)
            {
                var id = i.GetAttribute(Schema.vegetationSpawnMaterialType.MaterialIdAttribute);
                if (id is int)
                    yield return (int)id;
            }
        }

        public int GetNextMaterialId()
        {
            var maxId = -1;
            foreach (var id in GetMaterialIds())
                maxId = Math.Max(maxId, id);
            return maxId + 1;
        }

        #region IExportable
        public string ExportDirectory
        {
            get
            {
                var game = DomNode.GetRoot().As<Game.GameExtensions>();
                if (game != null) return game.ExportDirectory;
                var rootTerrain = DomNode.Parent.As<XLETerrainGob>();
                if (rootTerrain != null) return rootTerrain.CellsDirectory;
                return "";
            }
        }

        public string ExportTarget { get { return ExportDirectory + "/vegetationspawn.cfg"; } }
        public string ExportCategory { get { return "VegetationSpawn"; } }

        public IEnumerable<PendingExport> BuildPendingExports()
        {
            var result = new List<PendingExport>();
            result.Add(new PendingExport(ExportTarget, this.GetSceneManager().ExportVegetationSpawn(0)));
            return result;
        }
        #endregion

        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return false;

            switch ((Command)commandTag)
            {
                case Command.AddVegetationSpawnMaterial:
                    return true;
            }
            return false;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.AddVegetationSpawnMaterial:
                    {
                        var mat = new DomNode(Schema.vegetationSpawnMaterialType.Type);
                        mat.SetAttribute(Schema.vegetationSpawnMaterialType.MaterialIdAttribute, GetNextMaterialId());
                        ApplicationUtil.Insert(DomNode.GetRoot(), this, mat, "Add Vegetation Spawn Material", null);
                    }
                    break;
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }
        #endregion

        private enum Command
        {
            [Description("Add Vegetation Material")] AddVegetationSpawnMaterial
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in Enum.GetValues(typeof(Command)))
            {
                yield return command;
            }
        }

        public VegetationSpawnConfigGob() { }
    }

    class VegetationSpawnMaterialItem : DomNodeAdapter, IListable
    {
        public int Id
        {
            get
            {
                return GetAttribute<int>(Schema.vegetationSpawnMaterialType.MaterialIdAttribute);
            }
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Material: " + Id;
        }
    }
}

