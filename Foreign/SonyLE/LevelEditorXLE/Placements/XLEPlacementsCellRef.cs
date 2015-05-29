// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
using LevelEditorCore.GenericAdapters.Extensions;

using XLEBridgeUtils;

namespace LevelEditorXLE.Placements
{
    using CellRefST = Schema.placementsCellReferenceType;
    using FolderST = Schema.placementsFolderType;

    public class PlacementsCellRef 
        : LevelEditorCore.GenericAdapters.GenericReference<XLEPlacementDocument>
        , IHierarchical, IExportable
    {
        public Vec3F CaptureMins
        {
            get { return this.GetVec3(CellRefST.captureMinsAttribute); }
            set { this.SetVec3(CellRefST.captureMinsAttribute, value); }
        }

        public Vec3F CaptureMaxs
        {
            get { return this.GetVec3(CellRefST.captureMaxsAttribute); }
            set { this.SetVec3(CellRefST.captureMaxsAttribute, value); }
        }

        public Vec3F Offset
        {
            get { return this.GetVec3(CellRefST.offsetAttribute); }
            set { this.SetVec3(CellRefST.offsetAttribute, value); }
        }

        public string Name
        {
            get { return GetAttribute<string>(CellRefST.nameAttribute); }
            set { SetAttribute(CellRefST.nameAttribute, value); }
        }

        public Tuple<Vec3F, Vec3F> CalculateCellBoundary()
        {
            if (IsResolved())
            {
                    // if we are resolved, we can get the cell boundary from the
                    // native stuff
                var sceneMan = NativeManipulatorLayer.SceneManager;
                var target = Target.As<XLEBridgeUtils.INativeDocumentAdapter>();
                if (target != null)
                {
                    var boundary = GUILayer.EditorInterfaceUtils.CalculatePlacementCellBoundary(
                        sceneMan, target.NativeDocumentId);
                    var result = Tuple.Create(
                        Utils.AsVec3F(boundary.Item1),
                        Utils.AsVec3F(boundary.Item2));
                    this.SetVec3(CellRefST.cachedCellMinsAttribute, result.Item1);
                    this.SetVec3(CellRefST.cachedCellMaxsAttribute, result.Item2);
                    return result;
                }
            }

            return Tuple.Create(
                this.GetVec3(CellRefST.cachedCellMinsAttribute),
                this.GetVec3(CellRefST.cachedCellMaxsAttribute));
        }

        #region IExportable
        public string ExportTarget
        {
            get { return GetAttribute<string>(CellRefST.ExportTargetAttribute); }
            set { SetAttribute(CellRefST.ExportTargetAttribute, value); }
        }

        public string ExportCategory
        {
            get { return "Placements"; }
        }

        public GUILayer.EditorSceneManager.ExportResult PerformExport(string destinationFile)
        {
            if (!IsResolved())
                return new GUILayer.EditorSceneManager.ExportResult
                    { _success = false, _messages = "Cell reference is unresolved" };

            var target = Target.As<XLEBridgeUtils.INativeDocumentAdapter>();
            if (target == null)
                return new GUILayer.EditorSceneManager.ExportResult
                    { _success = false, _messages = "Error resolving target" };
                
            var sceneMan = NativeManipulatorLayer.SceneManager;
            return sceneMan.ExportPlacements(target.NativeDocumentId, destinationFile);
        }

        public GUILayer.EditorSceneManager.ExportPreview PreviewExport()
        {
            if (!IsResolved())
                return new GUILayer.EditorSceneManager.ExportPreview
                    { _success = false, _messages = "Cell reference is unresolved" };

            var target = Target.As<XLEBridgeUtils.INativeDocumentAdapter>();
            if (target == null)
                return new GUILayer.EditorSceneManager.ExportPreview { _success = false, _messages = "Error resolving target" };

            var sceneMan = NativeManipulatorLayer.SceneManager;
            return sceneMan.PreviewExportPlacements(target.NativeDocumentId);
        }
        #endregion

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
                uri, Globals.MEFContainer.GetExportedValue<ISchemaLoader>());
        }

        protected override void Detach(XLEPlacementDocument target)
        {
            XLEPlacementDocument.Release(target);
        }
        #endregion
    
        public static DomNode Create(string reference, string exportTarget, string name, Vec3F mins, Vec3F maxs)
        {
            var result = new DomNode(CellRefST.Type);
            result.SetAttribute(CellRefST.refAttribute, reference);
            result.SetAttribute(CellRefST.ExportTargetAttribute, exportTarget);
            result.SetAttribute(CellRefST.nameAttribute, name);
            DomNodeUtil.SetVector(result, CellRefST.captureMinsAttribute, mins);
            DomNodeUtil.SetVector(result, CellRefST.captureMaxsAttribute, maxs);
            return result;
        }
    }

    public class PlacementsFolder : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider, IExportable
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

        internal string BaseEditorPath
        {
            get { return GetAttribute<string>(FolderST.baseEditorPathAttribute); }
            set { SetAttribute(FolderST.baseEditorPathAttribute, value); }
        }

        internal string BaseExportPath
        {
            get { return GetAttribute<string>(FolderST.baseExportPathAttribute); }
            set { SetAttribute(FolderST.baseExportPathAttribute, value); }
        }

        internal int[] CellCount
        {
            get { return GetAttribute<int[]>(FolderST.cellCountAttribute); }
        }

        static private string FixPath(string input)
        {
            input.Replace('\\', '/');
            if (input.Length != 0 && input[input.Length - 1] != '/')
                input += "/";
            return input;
        }

        #region Configure Steps
        internal void Reconfigure(PlacementsConfig.Config cfg)
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

            BaseEditorPath = FixPath(cfg.BaseEditorPath);
            BaseExportPath = FixPath(cfg.BaseExportPath);
            SetAttribute(FolderST.cellCountAttribute, new int[2] { (int)cfg.CellCountX, (int)cfg.CellCountY } );
            SetAttribute(FolderST.cellSizeAttribute, cfg.CellSize);

            var newCells = new List<Tuple<string, string, string, Vec3F, Vec3F>>();
            var cellCount = CellCount;
            for (uint y=0; y<cellCount[1]; ++y)
                for (uint x = 0; x < cellCount[0]; ++x) {
                    var rf = String.Format(
                        "{0}p{1,3:D3}_{2,3:D3}.plcdoc",
                        BaseEditorPath, x, y);
                    var ex = String.Format(
                        "{0}p{1,3:D3}_{2,3:D3}.plc",
                        BaseExportPath, x, y);
                    var name = String.Format("{0,2:D2}-{1,2:D2}", x, y);
                    var mins = new Vec3F(x * cfg.CellSize, y * cfg.CellSize, -5000.0f);
                    var maxs = new Vec3F((x+1) * cfg.CellSize, (y+1) * cfg.CellSize, 5000.0f);
                    newCells.Add(Tuple.Create(rf, ex, name, mins, maxs));
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
                    r.ExportTarget = newCells[index].Item2; 
                    r.Name = newCells[index].Item3;
                    r.CaptureMins = newCells[index].Item4;
                    r.CaptureMaxs = newCells[index].Item5;
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
                        PlacementsCellRef.Create(
                            newCells[c].Item1, newCells[c].Item2, newCells[c].Item3, 
                            newCells[c].Item4, newCells[c].Item4));
        }

        internal bool DoModalConfigure()
        {
                // open the configuration dialog
            var cfg = new PlacementsConfig.Config();
            cfg.BaseEditorPath = BaseEditorPath;
            cfg.BaseExportPath = BaseExportPath;
            var cellCount = CellCount;
            cfg.CellCountX = (uint)cellCount[0];
            cfg.CellCountY = (uint)cellCount[1];
            cfg.CellSize = GetAttribute<float>(FolderST.cellSizeAttribute);

            using (var dlg = new PlacementsConfig())
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
        #endregion

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

        #region IExportable
        public string ExportTarget
        {
            get { return GetAttribute<string>(FolderST.ExportTargetAttribute); }
            set { SetAttribute(FolderST.ExportTargetAttribute, value); }
        }

        public string ExportCategory { get { return "Placements"; } }

        public GUILayer.EditorSceneManager.ExportResult PerformExport(string destinationFile)
        {
            var sceneMan = NativeManipulatorLayer.SceneManager;
            return sceneMan.ExportPlacementsCfg(BuildExportRefs(), destinationFile);
        }

        public GUILayer.EditorSceneManager.ExportPreview PreviewExport()
        {
            var sceneMan = NativeManipulatorLayer.SceneManager;
            return sceneMan.PreviewExportPlacementsCfg(BuildExportRefs());
        }

        private IEnumerable<GUILayer.EditorSceneManager.PlacementCellRef> BuildExportRefs()
        {
            var refs = new List<GUILayer.EditorSceneManager.PlacementCellRef>();
            foreach (var c in Cells)
            {
                var boundary = c.CalculateCellBoundary();
                refs.Add(new GUILayer.EditorSceneManager.PlacementCellRef
                    {
                        NativeFile = c.ExportTarget,
                        Offset = Utils.AsVector3(c.Offset),
                        Mins = Utils.AsVector3(boundary.Item1),
                        Maxs = Utils.AsVector3(boundary.Item2)
                    });
            }
            return refs;
        }
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

