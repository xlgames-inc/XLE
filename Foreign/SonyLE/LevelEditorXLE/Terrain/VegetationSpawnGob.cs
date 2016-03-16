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
    class VegetationSpawnMaterialItem : DomNodeAdapter, IListable, INameable
    {
        public int Id
        {
            get { return GetAttribute<int>(Schema.vegetationSpawnMaterialType.MaterialIdAttribute); }
            set { SetAttribute(Schema.vegetationSpawnMaterialType.MaterialIdAttribute, value); }
        }

        public string Name
        {
            get { return GetAttribute<string>(Schema.vegetationSpawnMaterialType.NameAttribute); }
            set { SetAttribute(Schema.vegetationSpawnMaterialType.NameAttribute, value); }
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = Name;
        }

        public static DomNode Create(int materialId)
        {
            var mat = new DomNode(Schema.vegetationSpawnMaterialType.Type);
            mat.SetAttribute(Schema.vegetationSpawnMaterialType.MaterialIdAttribute, materialId);
            mat.SetAttribute(Schema.vegetationSpawnMaterialType.NameAttribute, "Material: " + materialId.ToString());
            return mat;
        }
    }

    class VegetationSpawnConfigGob
        : DomNodeAdapter
        , IHierarchical, IExportable, ICommandClient, IContextMenuCommandProvider
    {
        #region IHierarchical members
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
        #endregion

        #region Material Children
        IEnumerable<VegetationSpawnMaterialItem> Materials
        {
            get { return GetChildList<DomNode>(Schema.vegetationSpawnConfigType.materialChild).AsIEnumerable<VegetationSpawnMaterialItem>(); }
        }

        public IEnumerable<int> GetMaterialIds()
        {
            foreach (var i in Materials)
                yield return i.Id;
        }

        public int GetNextMaterialId()
        {
            var maxId = -1;
            foreach (var id in GetMaterialIds())
                maxId = Math.Max(maxId, id);
            return maxId + 1;
        }

        protected override void OnNodeSet()
        {
            DomNode.ChildInserted += DomNode_ChildInserted;
            DomNode.ChildRemoved += DomNode_ChildRemoved;
            UpdateBridgeList();
        }

        private void DomNode_ChildInserted(object sender, ChildEventArgs e)         { e.Child.AttributeChanged += Child_AttributeChanged; UpdateBridgeList(); }
        private void DomNode_ChildRemoved(object sender, ChildEventArgs e)          { e.Child.AttributeChanged -= Child_AttributeChanged; UpdateBridgeList(); }
        private void Child_AttributeChanged(object sender, AttributeEventArgs e)    { UpdateBridgeList(); }

        private void UpdateBridgeList()
        {
            var bridge = Globals.MEFContainer.GetExport<TerrainNamingBridge>().Value;
            var names = new List<Tuple<string, int>>();
            foreach (var m in Materials)
                names.Add(new Tuple<string, int>(m.Name, m.Id));
            bridge.SetDecorationMaterials(names);
        }
        #endregion

        #region IExportable
        public string ExportDirectory
        {
            get
            {
                var game = DomNode.GetRoot().As<Game.GameExtensions>();
                if (game != null) return game.ExportDirectory;
                var rootTerrain = DomNode.Parent.As<XLETerrainGob>();
                if (rootTerrain != null) return rootTerrain.CellsDirectory.LocalPath;
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
                        var mat = VegetationSpawnMaterialItem.Create(GetNextMaterialId());
                        ApplicationUtil.Insert(DomNode.GetRoot(), this, mat, "Add Decoration Material", null);
                    }
                    break;
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }
        #endregion

        private enum Command
        {
            [Description("Add Decoration Material")] AddVegetationSpawnMaterial
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in Enum.GetValues(typeof(Command)))
            {
                yield return command;
            }
        }
    }
}

