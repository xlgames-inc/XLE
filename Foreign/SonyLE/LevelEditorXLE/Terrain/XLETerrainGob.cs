// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;

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

    class XLETerrainGob : DomNodeAdapter, IListable, ICommandClient, IContextMenuCommandProvider
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

        private static uint ClampNodeDimensions(uint input)
        {
            return input.Clamp(1u, 1024u);
        }

        private static uint ClampCellTreeDepth(uint input)
        {
            return input.Clamp(1u, 16u);
        }

        #region Configure Steps
        internal void Reconfigure(TerrainConfig.Config cfg)
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;

            var nodeDimensions = ClampNodeDimensions(cfg.NodeDimensions);
            var overlap = cfg.Overlap;
            var cellTreeDepth = ClampCellTreeDepth(cfg.CellTreeDepth);

                // if there is a source DEM file specified then we should
                // attempt to build the starter uber surface.
            if (cfg.Import == TerrainConfig.Config.ImportType.DEMFile && cfg.SourceDEMFile.Length > 0)
            {
                GUILayer.EditorInterfaceUtils.GenerateUberSurfaceFromDEM(
                    cfg.UberSurfaceDirectory, cfg.SourceDEMFile, 
                    nodeDimensions, cellTreeDepth);
            }

                // fill in the cells directory with starter cells (if they don't already exist)
            GUILayer.EditorInterfaceUtils.GenerateStarterCells(
                cfg.CellsDirectory, cfg.UberSurfaceDirectory,
                nodeDimensions, cellTreeDepth, overlap);

                // if the above completed without throwing an exception, we can commit the values
            NodeDimensions = nodeDimensions;
            Overlap = overlap;
            CellTreeDepth = cellTreeDepth;
            Spacing = cfg.Spacing;
            UberSurfaceDirectory = cfg.UberSurfaceDirectory;
            CellsDirectory = cfg.CellsDirectory;
        }

        internal bool DoModalConfigure()
        {
            // open the configuration dialog
            var cfg = new TerrainConfig.Config();
            cfg.NodeDimensions = NodeDimensions;
            cfg.Overlap = Overlap;
            cfg.Spacing = Spacing;
            cfg.CellTreeDepth = CellTreeDepth;
            cfg.UberSurfaceDirectory = UberSurfaceDirectory;
            cfg.CellsDirectory = CellsDirectory;

            using (var dlg = new TerrainConfig())
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
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.CreateBaseTexture:
                        return DomNode.GetChild(TerrainST.baseTextureChild) == null;
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
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }
        #endregion

        private enum Command
        {
            [Description("Create Base Texture")]
            CreateBaseTexture
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
