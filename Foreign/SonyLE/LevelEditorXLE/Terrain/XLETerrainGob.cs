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

namespace LevelEditorXLE.Terrain
{
    using TerrainST = Schema.terrainType;

    static class Helpers
    {
        public static T Clamp<T>(this T val, T min, T max) where T : IComparable<T>
        {
            if (val.CompareTo(min) < 0) return min;
            else if (val.CompareTo(max) > 0) return max;
            else return val;
        }
    }

    class XLETerrainGob : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider, IExportable, IHierarchical
    {
        public XLETerrainGob() {}
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Terrain";
        }

        public static DomNode Create()
        {
            return new DomNode(TerrainST.Type);
        }

        public static DomNode CreateWithConfigure()
        {
            var result = new DomNode(TerrainST.Type);
            var adapter = result.As<XLETerrainGob>();
            if (adapter != null && adapter.DoModalConfigure())
                return result;
            return null;
        }

        public string UberSurfaceDirectory
        {
            get { return GetAttribute<string>(TerrainST.UberSurfaceDirAttribute); }
            set { SetAttribute(TerrainST.UberSurfaceDirAttribute, value); }
        }

        public string CellsDirectory
        {
            get { return GetAttribute<string>(TerrainST.CellsDirAttribute); }
            set { SetAttribute(TerrainST.CellsDirAttribute, value); }
        }

        public uint NodeDimensions
        {
            get { return GetAttribute<uint>(TerrainST.NodeDimensionsAttribute); }
            set { SetAttribute(TerrainST.NodeDimensionsAttribute, ClampNodeDimensions(value)); }
        }

        public uint Overlap
        {
            get { return GetAttribute<uint>(TerrainST.OverlapAttribute); }
            set { SetAttribute(TerrainST.OverlapAttribute, value); }
        }

        public float Spacing
        {
            get { return GetAttribute<float>(TerrainST.SpacingAttribute); }
            set { SetAttribute(TerrainST.SpacingAttribute, value); }
        }

        public uint CellTreeDepth
        {
            get { return GetAttribute<uint>(TerrainST.CellTreeDepthAttribute); }
            set { SetAttribute(TerrainST.CellTreeDepthAttribute, ClampCellTreeDepth(value)); }
        }

        public bool HasBaseMaterialCoverage
        {
            get { return GetAttribute<bool>(TerrainST.HasBaseMaterialCoverageAttribute); }
            set { SetAttribute(TerrainST.HasBaseMaterialCoverageAttribute, value); }
        }

        public bool HasDecorationCoverage
        {
            get { return GetAttribute<bool>(TerrainST.HasDecorationCoverageAttribute); }
            set { SetAttribute(TerrainST.HasDecorationCoverageAttribute, value); }
        }

        public bool HasShadowsCoverage
        {
            get { return GetAttribute<bool>(TerrainST.HasShadowsConverageAttribute); }
            set { SetAttribute(TerrainST.HasShadowsConverageAttribute, value); }
        }

        public bool HasEncodedGradientFlags
        {
            get { return GetAttribute<bool>(TerrainST.HasEncodedGradientFlagsAttribute); }
            set { SetAttribute(TerrainST.HasEncodedGradientFlagsAttribute, value); }
        }

        public float SunPathAngle
        {
            get { return GetAttribute<float>(TerrainST.SunPathAngleAttribute); }
            set { SetAttribute(TerrainST.SunPathAngleAttribute, value); }
        }

        public uint[] CellCount
        {
            get { return GetAttribute<uint[]>(TerrainST.CellCountAttribute); }
            set { SetAttribute(TerrainST.CellCountAttribute, value); }
        }

        public float GradFlagSlopeThreshold
        {
            get { return GetAttribute<float>(TerrainST.GradFlagSlopeThresholdAttribute); }
            set { SetAttribute(TerrainST.GradFlagSlopeThresholdAttribute, value); }
        }

        public float GradFlagRoughThreshold
        {
            get { return GetAttribute<float>(TerrainST.GradFlagRoughThresholdAttribute); }
            set { SetAttribute(TerrainST.GradFlagRoughThresholdAttribute, value); }
        }

        private static uint ClampNodeDimensions(uint input)
        {
            return input.Clamp(1u, 1024u);
        }

        private static uint ClampCellTreeDepth(uint input)
        {
            return input.Clamp(1u, 16u);
        }

        public bool CanAddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode == null) return false;

            return domNode.Type.Lineage.FirstOrDefault(t => t == Schema.vegetationSpawnConfigType.Type) != null;
        }

        public bool AddChild(object child)
        {
            var domNode = child.As<DomNode>();
            if (domNode != null && domNode.Type.Lineage.FirstOrDefault(t => t == Schema.vegetationSpawnConfigType.Type) != null)
            {
                SetChild(Schema.terrainType.VegetationSpawnChild, domNode);
                return true;
            }

            return false;
        }

        #region IExportable
        public string ExportTarget
        {
            get { return CellsDirectory + "/cached.dat"; }
            set { throw new NotImplementedException("XLETerrainGob.ExportTarget.Set"); }
        }

        public string ExportCategory
        {
            get { return "Terrain"; }
        }

        public GUILayer.EditorSceneManager.ExportResult PerformExport(string destinationFile)
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            return sceneMan.ExportTerrainCachedData(destinationFile);
        }

        public GUILayer.EditorSceneManager.ExportPreview PreviewExport()
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            return sceneMan.PreviewExportTerrainCachedData();
        }
        #endregion

        #region Configure Steps
        internal TerrainConfig.Config BuildDialogConfig()
        {
            var cfg = new TerrainConfig.Config();
            cfg.NodeDimensions = NodeDimensions;
            cfg.Overlap = Overlap;
            cfg.Spacing = Spacing;
            cfg.CellTreeDepth = CellTreeDepth;
            cfg.UberSurfaceDirectory = UberSurfaceDirectory;
            cfg.CellsDirectory = CellsDirectory;
            cfg.HasBaseMaterialCoverage = HasBaseMaterialCoverage;
            cfg.HasDecorationCoverage = HasDecorationCoverage;
            cfg.HasShadowsCoverage = HasShadowsCoverage;
            cfg.HasEncodedGradientFlags = HasEncodedGradientFlags;
            cfg.SunPathAngle = SunPathAngle;
            cfg.SlopeThreshold = GradFlagSlopeThreshold;
            cfg.RoughThreshold = GradFlagRoughThreshold;
            return cfg;
        }

        internal void CommitDialogConfig(TerrainConfig.Config cfg)
        {
            NodeDimensions = cfg.NodeDimensions;
            Overlap = cfg.Overlap;
            CellTreeDepth = cfg.CellTreeDepth;
            Spacing = cfg.Spacing;
            UberSurfaceDirectory = cfg.UberSurfaceDirectory;
            CellsDirectory = cfg.CellsDirectory;
            HasDecorationCoverage = cfg.HasDecorationCoverage;
            HasBaseMaterialCoverage = cfg.HasBaseMaterialCoverage;
            HasShadowsCoverage = cfg.HasShadowsCoverage;
            HasEncodedGradientFlags = cfg.HasEncodedGradientFlags;
            SunPathAngle = cfg.SunPathAngle;
            GradFlagSlopeThreshold = cfg.SlopeThreshold;
            GradFlagRoughThreshold = cfg.RoughThreshold;
        }

        internal GUILayer.TerrainConfig BuildEngineConfig(TerrainConfig.Config cfg)
        {
            var result = new GUILayer.TerrainConfig(
                cfg.CellsDirectory,
                cfg.NodeDimensions, cfg.CellTreeDepth, cfg.Overlap,
                cfg.Spacing, (float)(cfg.SunPathAngle * Math.PI / 180.0f),
                cfg.HasEncodedGradientFlags);

            result.CellCount = new GUILayer.VectorUInt2(CellCount[0], CellCount[1]);

            if (cfg.HasBaseMaterialCoverage)
                result.Add(GUILayer.EditorInterfaceUtils.DefaultCoverageLayer(result, cfg.UberSurfaceDirectory, 1000));
            if (cfg.HasDecorationCoverage)
                result.Add(GUILayer.EditorInterfaceUtils.DefaultCoverageLayer(result, cfg.UberSurfaceDirectory, 1001));
            if (cfg.HasShadowsCoverage)
                result.Add(GUILayer.EditorInterfaceUtils.DefaultCoverageLayer(result, cfg.UberSurfaceDirectory, 2));

            return result;
        }

        internal GUILayer.TerrainConfig BuildEngineConfig()
        {
            return BuildEngineConfig(BuildDialogConfig());
        }

        internal void Reconfigure(TerrainConfig.Config cfg)
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;

            cfg.NodeDimensions = ClampNodeDimensions(cfg.NodeDimensions);
            cfg.CellTreeDepth = ClampCellTreeDepth(cfg.CellTreeDepth);

            sceneMan.UnloadTerrain();

            try
            {
                var newCellCount = new GUILayer.VectorUInt2(0, 0);
                using (var progress = new ControlsLibrary.ProgressDialog.ProgressInterface())
                {
                    // if there is a source DEM file specified then we should
                    // attempt to build the starter uber surface.
                    if (cfg.Import == TerrainConfig.Config.ImportType.DEMFile
                        && cfg.SourceDEMFile != null && cfg.SourceDEMFile.Length > 0)
                    {
                        GUILayer.EditorInterfaceUtils.GenerateUberSurfaceFromDEM(
                            cfg.UberSurfaceDirectory, cfg.SourceDEMFile,
                            cfg.NodeDimensions, cfg.CellTreeDepth,
                            progress);
                    } else if (cfg.Import == TerrainConfig.Config.ImportType.NewBlankTerrain
                        && cfg.NewCellCountX != 0 && cfg.NewCellCountY != 0)
                    {
                        GUILayer.EditorInterfaceUtils.GenerateBlankUberSurface(
                            cfg.UberSurfaceDirectory, cfg.NewCellCountX, cfg.NewCellCountY,
                            cfg.NodeDimensions, cfg.CellTreeDepth,
                            progress);
                    }

                    var engineCfg = BuildEngineConfig(cfg);
                    engineCfg.InitCellCountFromUberSurface(cfg.UberSurfaceDirectory);
                    newCellCount = engineCfg.CellCount;

                        // fill in the cells directory with starter cells (if they don't already exist)
                        // (and build empty uber surface files for any that are missing)
                    GUILayer.EditorInterfaceUtils.GenerateMissingUberSurfaceFiles(
                        BuildEngineConfig(), UberSurfaceDirectory, progress); 
                    GUILayer.EditorInterfaceUtils.GenerateCellFiles(
                        BuildEngineConfig(), UberSurfaceDirectory, false,
                        cfg.SlopeThreshold, cfg.RoughThreshold,
                        progress);

                        // native side reads the config from disk atm.
                        // so we need to commit to disk
                    engineCfg.Save();
                }

                    // if the above completed without throwing an exception, we can commit the values
                CommitDialogConfig(cfg);
                CellCount = new uint[2] { newCellCount.X, newCellCount.Y };
            }
            catch { }

            sceneMan.ReloadTerrain();
        }

        internal bool DoModalConfigure()
        {
                // open the configuration dialog
            using (var dlg = new TerrainConfig())
            {
                dlg.Value = BuildDialogConfig();
                var result = dlg.ShowDialog();
                if (result == System.Windows.Forms.DialogResult.OK)
                {
                    Reconfigure(dlg.Value);
                    return true;
                }
            }
            return false;
        }
        #endregion

        void DoGenerateShadows()
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            sceneMan.UnloadTerrain();

            try
            {
                using (var progress = new ControlsLibrary.ProgressDialog.ProgressInterface())
                {
                    GUILayer.EditorInterfaceUtils.GenerateShadowsSurface(
                        BuildEngineConfig(), UberSurfaceDirectory,
                        progress);
                }
            }
            catch { }

            sceneMan.ReloadTerrain();
        }

        void DoFlushToDisk()
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            using (var progress = new ControlsLibrary.ProgressDialog.ProgressInterface())
            {
                GUILayer.EditorInterfaceUtils.FlushTerrainToDisk(sceneMan, progress);
            }
        }

        void DoRebuildCellFiles()
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            sceneMan.UnloadTerrain();

            try
            {
                using (var progress = new ControlsLibrary.ProgressDialog.ProgressInterface())
                {
                    GUILayer.EditorInterfaceUtils.GenerateCellFiles(
                        BuildEngineConfig(), UberSurfaceDirectory, true,
                        GradFlagSlopeThreshold, GradFlagRoughThreshold, progress);
                }
            }
            catch { }

            sceneMan.ReloadTerrain();
        }

        #region ICommandClient Members
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.CreateBaseTexture:
                        return DomNode.GetChild(TerrainST.baseTextureChild) == null;

                    case Command.Configure:
                    case Command.GenerateShadows:
                    case Command.FlushToDisk:
                    case Command.RebuildCellFiles:
                        return true;
                }
            }
            return false;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.CreateBaseTexture:
                    {
                        if (DomNode.GetChild(TerrainST.baseTextureChild) == null)
                        {
                            DomNode.SetChild(
                                TerrainST.baseTextureChild,
                                new DomNode(Schema.terrainBaseTextureType.Type));
                        }
                        break;
                    }

                case Command.Configure:
                    {
                        DoModalConfigure();
                        break;
                    }

                case Command.GenerateShadows:
                    {
                        DoGenerateShadows();
                        break;
                    }

                case Command.FlushToDisk:
                    {
                        DoFlushToDisk();
                        break;
                    }

                case Command.RebuildCellFiles:
                    {
                        DoRebuildCellFiles();
                        break;
                    }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }
        #endregion

        private enum Command
        {
            [Description("Create Base Texture")]
            CreateBaseTexture,
            [Description("Configure Terrain...")]
            Configure,
            [Description("Generate Shadows")]
            GenerateShadows,
            [Description("Commit to disk")]
            FlushToDisk,
            [Description("Rebuild cell files")]
            RebuildCellFiles
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
