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
    class VegetationSpawnConfigGob : DomNodeAdapter, IHierarchical, IListable, IExportable
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
            info.Label = "VegetationSpawnConfig";
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

        public VegetationSpawnConfigGob() { }
    }
}

