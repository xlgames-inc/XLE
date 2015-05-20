//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore;

using LevelEditor.DomNodeAdapters.Extensions;

namespace LevelEditor.DomNodeAdapters
{
    using CellRefST = Schema.placementsCellReferenceType;
    using FolderST = Schema.placementsFolderType;

    public class PlacementsCellRef : GenericReference<XLEPlacementDocument>, IHierarchical
    {
        public Vec3F Mins
        {
            get { return this.GetVec3(CellRefST.minsAttribute); }
            set { this.SetVec3(CellRefST.minsAttribute, value); }
        }

        public Vec3F Maxs
        {
            get { return this.GetVec3(CellRefST.maxsAttribute); }
            set { this.SetVec3(CellRefST.maxsAttribute, value); }
        }

        public string Name
        {
            get { return GetAttribute<string>(CellRefST.nameAttribute); }
            set { SetAttribute(CellRefST.nameAttribute, value); }
        }

        #region IHierachical Members
        public bool CanAddChild(object child) { return (m_target != null) && m_target.CanAddChild(child); }
        public bool AddChild(object child) { return (m_target != null) && m_target.AddChild(child); }
        #endregion
        #region GenericReference<> Members
        static PlacementsCellRef()
        {
            s_nameAttribute = CellRefST.nameAttribute;
            s_refAttribute = CellRefST.refAttribute;
        }

        protected override XLEPlacementDocument Attach(Uri uri)
        {
            return XLEPlacementDocument.OpenOrCreate(
                uri, Globals.MEFContainer.GetExportedValue<SchemaLoader>());
        }

        protected override void Detach(XLEPlacementDocument target)
        {
            XLEPlacementDocument.Release(target);
        }
        #endregion
    
        public static DomNode Create(string reference, string name, Vec3F mins, Vec3F maxs)
        {
            var result = new DomNode(CellRefST.Type);
            result.SetAttribute(CellRefST.refAttribute, reference);
            result.SetAttribute(CellRefST.nameAttribute, name);
            DomNodeUtil.SetVector(result, CellRefST.minsAttribute, mins);
            DomNodeUtil.SetVector(result, CellRefST.maxsAttribute, maxs);
            return result;
        }
    }

    public class PlacementsFolder : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider
    {
        public static DomNode Create()
        {
            return new DomNode(FolderST.Type);
        }

        public static DomNode CreateWithConfigure()
        {
            var result = new DomNode(FolderST.Type);
            var adapter = result.As<PlacementsFolder>();
            if (adapter != null && adapter.DoModalConfigure())
                return result;
            return null;
        }

        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placements";
        }

        public System.Collections.Generic.IEnumerable<PlacementsCellRef> Cells
        {
            get { return GetChildList<PlacementsCellRef>(FolderST.cellChild); }
        }

        internal void Reconfigure(XLEControls.PlacementsConfig.Config cfg)
        {
                // Based on the configuration settings given, remove and attach
                // cell references. First create a list of all of the references
                // we want to see.
                // We can use this to filter out the existing references that should
                // be removed. 
                // Then, we should create new references for all new cells that have
                // been added.
                // Cells always have the same basic filename structure, and are
                // arranged in a dense 2d grid. The filenames contain the x and y
                // coordinates of the cell in that grid.

            var basePath = cfg.BasePath;
            basePath.Replace('\\', '/');
            if (basePath.Length != 0 && basePath[basePath.Length-1] != '/')
                basePath += "/";
            SetAttribute(FolderST.basePathAttribute, basePath);
            SetAttribute(FolderST.cellCountAttribute, new int[2] { (int)cfg.CellCountX, (int)cfg.CellCountY } );
            SetAttribute(FolderST.cellSizeAttribute, cfg.CellSize);

            var newCells = new List<Tuple<string, string, Vec3F, Vec3F>>();
            var cellCount = CellCount;
            for (uint y=0; y<cellCount[1]; ++y)
                for (uint x = 0; x < cellCount[0]; ++x) {
                    var rf = String.Format(
                        "{0}p{1,3:D3}_{2,3:D3}.plcdoc", 
                        basePath, x, y);
                    var name = String.Format("{0,2:D2}-{1,2:D2}", x, y);
                    var mins = new Vec3F(x * cfg.CellSize, y * cfg.CellSize, -5000.0f);
                    var maxs = new Vec3F((x+1) * cfg.CellSize, (y+1) * cfg.CellSize, 5000.0f);
                    newCells.Add(Tuple.Create(rf, name, mins, maxs));
                }

            var foundMatching = new bool[newCells.Count];
            Array.Clear(foundMatching, 0, foundMatching.Length);

            var toBeRemoved = new List<PlacementsCellRef>();
            var existingRefs = Cells;
            foreach(var r in existingRefs) {
                var rawRef = r.RawReference;
                int index = newCells.FindIndex(0, newCells.Count, s => s.Item1.Equals(rawRef));
                if (index >= 0) {
                    foundMatching[index] = true;
                    r.Name = newCells[index].Item2;
                    r.Mins = newCells[index].Item3;
                    r.Maxs = newCells[index].Item4;
                } else
                    toBeRemoved.Add(r);
            }

                // \todo -- if we have some cell references to be removed, we should first check if there
                // are any changes, and if they need to be saved!
            foreach (var r in toBeRemoved)
            {
                r.Unresolve();
                r.DomNode.RemoveFromParent();
            }

                // Create the new cell references that are needed...
            var childList = DomNode.GetChildList(Schema.placementsFolderType.cellChild);
            for (int c = 0; c < newCells.Count; ++c)
                if (!foundMatching[c])
                    childList.Add(
                        PlacementsCellRef.Create(newCells[c].Item1, newCells[c].Item2, newCells[c].Item3, newCells[c].Item4));
        }

        internal string BasePath
        {
            get { return GetAttribute<string>(FolderST.basePathAttribute); }
        }

        internal int[] CellCount
        {
            get { return GetAttribute<int[]>(FolderST.cellCountAttribute); }
        }

        internal bool DoModalConfigure()
        {
                // open the configuration dialog
            var cfg = new XLEControls.PlacementsConfig.Config();
            cfg.BasePath = BasePath;
            var cellCount = CellCount;
            cfg.CellCountX = (uint)cellCount[0];
            cfg.CellCountY = (uint)cellCount[1];
            cfg.CellSize = GetAttribute<float>(FolderST.cellSizeAttribute);

            using (var dlg = new XLEControls.PlacementsConfig())
            {
                dlg.Value = cfg;
                var result = dlg.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    Reconfigure(cfg);
                    return true;
                }
            }
            return false;
        }

        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            return commandTag is Command;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
            case Command.Configure:
                {
                    DoModalConfigure();
                    break;
                }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        {}
        #endregion

        private enum Command
        {
            [Description("Configure Placements...")]
            Configure
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

