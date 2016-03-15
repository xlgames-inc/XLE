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
    class TerrainBaseTextureMaterial : DomNodeAdapter, IListable, INameable
    {
        public int Id
        {
            get { return GetAttribute<int>(Schema.abstractTerrainMaterialDescType.MaterialIdAttribute); }
            set { SetAttribute(Schema.abstractTerrainMaterialDescType.MaterialIdAttribute, value); }
        }

        public string Name
        {
            get { return GetAttribute<string>(Schema.abstractTerrainMaterialDescType.NameAttribute); }
            set { SetAttribute(Schema.abstractTerrainMaterialDescType.NameAttribute, value); }
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = Name;
        }

        public static DomNode Create(int materialId)
        {
            var mat = new DomNode(Schema.terrainMaterialType.Type);
            mat.SetAttribute(Schema.abstractTerrainMaterialDescType.MaterialIdAttribute, materialId);
            mat.SetAttribute(Schema.abstractTerrainMaterialDescType.NameAttribute, "Material: " + materialId.ToString());
            return mat;
        }
    }

    class TerrainBaseTexture
        : DomNodeAdapter
        , IExportable, ICommandClient, IContextMenuCommandProvider, IHierarchical
    {
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

        #region IHierarchical members
        private static bool IsDerivedFrom(DomNode node, DomNodeType type) { return node.Type.Lineage.FirstOrDefault(t => t == type) != null; }

        public bool CanAddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode == null) return false;
            return IsDerivedFrom(domNode, Schema.abstractTerrainMaterialDescType.Type);
        }
        public bool AddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode != null && IsDerivedFrom(domNode, Schema.abstractTerrainMaterialDescType.Type))
            {
                GetChildList<DomNode>(Schema.terrainBaseTextureType.materialChild).Add(domNode);
                return true;
            }

            return false;
        }
        #endregion

        #region Material Children
        IEnumerable<TerrainBaseTextureMaterial> Materials
        {
            get { return GetChildList<DomNode>(Schema.terrainBaseTextureType.materialChild).AsIEnumerable<TerrainBaseTextureMaterial>(); }
        }

        public IEnumerable<int> GetMaterialIds()
        {
            foreach(var i in Materials)
                yield return i.Id;
        }

        public int GetNextMaterialId()
        {
            var maxId = -1;
            foreach (var id in GetMaterialIds())
                maxId = Math.Max(maxId, id);
            return maxId+1;
        }

        protected override void OnNodeSet()
        {
            DomNode.ChildInserted += DomNode_ChildInserted;
            DomNode.ChildRemoved += DomNode_ChildRemoved;
            UpdateBridgeList();
        }

        private void DomNode_ChildInserted(object sender, ChildEventArgs e)         { e.Child.AttributeChanged += Child_AttributeChanged; UpdateBridgeList();}
        private void DomNode_ChildRemoved(object sender, ChildEventArgs e)          { e.Child.AttributeChanged -= Child_AttributeChanged; UpdateBridgeList(); }
        private void Child_AttributeChanged(object sender, AttributeEventArgs e)    { UpdateBridgeList(); }

        private void UpdateBridgeList()
        {
            var bridge = Globals.MEFContainer.GetExport<TerrainNamingBridge>().Value;
            var names = new List<Tuple<string, int>>();
            foreach(var m in Materials)
                names.Add(new Tuple<string,int>(m.Name, m.Id));
            bridge.SetBaseTextureMaterials(names);
        }
        #endregion

        #region IExportable
        public string ExportTarget
        {
            get { return ExportDirectory + "TerrainMaterial.cfg"; }
        }

        public string ExportCategory
        {
            get { return "Terrain"; }
        }

        public IEnumerable<PendingExport> BuildPendingExports()
        {
            var result = new List<PendingExport>();
            result.Add(new PendingExport(ExportTarget, this.GetSceneManager().ExportTerrainMaterialData()));
            return result;
        }
        #endregion

        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return false;

            switch ((Command)commandTag)
            {
                case Command.AddTerrainTextureMaterial:
                    return true;
            }
            return false;
        }
        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.AddTerrainTextureMaterial:
                    {
                        var mat = TerrainBaseTextureMaterial.Create(GetNextMaterialId());
                        ApplicationUtil.Insert(
                            DomNode.GetRoot(), this, mat, 
                            "Add Terrain Material", null);
                    }
                    break;
            }
        }
        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState) {}
        #endregion

        private enum Command
        {
            [Description("Add Material")] AddTerrainTextureMaterial
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in Enum.GetValues(typeof(Command)))
                yield return command;
        }
    }
}
