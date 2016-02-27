// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace LevelEditorXLE.Terrain
{
    public partial class TerrainConfig : Form
    {
        public TerrainConfig()
        {
            InitializeComponent();
        }

        public Config Value
        {
            get { return m_config; }
            set
            {
                m_config = value;
                m_propertyGrid1.SelectedObject = value;

                m_importType.Enabled = value.Import == Config.ImportType.DEMFile;
                m_importSource.Enabled = value.Import == Config.ImportType.DEMFile;
                m_importSourceBtn.Enabled = value.Import == Config.ImportType.DEMFile;
                m_importType.SelectedIndex = 0;
                m_importSource.Text = value.SourceDEMFile;

                m_createCellsX.Text = value.NewCellCountX.ToString();
                m_createCellsY.Text = value.NewCellCountY.ToString();
                m_createCellsX.Enabled = value.Import == Config.ImportType.NewBlankTerrain;
                m_createCellsY.Enabled = value.Import == Config.ImportType.NewBlankTerrain;

                UpdateImportOp();
            }
        }

        public class Config
        {
            [Category("Basic")] [Description("Size of the nodes, in elements (typically 32)")]
            public uint NodeDimensions { get; set; }

            [Category("Basic")] [Description("Overlap room between nodes (typically 2)")]
            public uint Overlap { get; set; }

            [Category("Basic")] [Description("Depth of the tree of LODs in a cell (typically 5)")]
            public uint CellTreeDepth { get; set; }

            [Category("Basic")] [Description("Space between each element (in metres)")]
            public float Spacing { get; set; }

            [Category("Files")] [Description("UberSurface directory")]
            [EditorAttribute(typeof(System.Windows.Forms.Design.FolderNameEditor), typeof(System.Drawing.Design.UITypeEditor))]
            public string UberSurfaceDirectory { get; set; }

            [Category("Files")] [Description("Cells directory")]
            [EditorAttribute(typeof(System.Windows.Forms.Design.FolderNameEditor), typeof(System.Drawing.Design.UITypeEditor))]
            public string CellsDirectory { get; set; }

            [Category("Shadows")] [Description("Sun Path Angle (in degrees)")]
            public float SunPathAngle { get; set; }
            [Category("Gradient Flags")] [Description("Has encoded gradient flags")]
            public bool HasEncodedGradientFlags { get; set; }

            [Category("Gradient Flags")] [Description("Threshold for flat texturing")]
            public float SlopeThreshold0 { get; set; }

            [Category("Gradient Flags")] [Description("Threshold for first slope texturing")]
            public float SlopeThreshold1 { get; set; }

            [Category("Gradient Flags")] [Description("Threshold for second slope texturing")]
            public float SlopeThreshold2 { get; set; }

            [Browsable(false)] public string SourceDEMFile { get; set; }
            [Browsable(false)] public uint NewCellCountX { get; set; }
            [Browsable(false)] public uint NewCellCountY { get; set; }

            public enum ImportType { None, DEMFile, NewBlankTerrain };

            [Browsable(false)] public ImportType Import { get; set; }
            [Browsable(false)] public GUILayer.TerrainImportOp ImportOp { get; set; }

            internal Config() 
            {
                NodeDimensions = 32; Overlap = 2; Spacing = 10; Import = ImportType.None;
                CellTreeDepth = 5;
                NewCellCountX = NewCellCountY = 1;
                HasEncodedGradientFlags = false;
                SunPathAngle = 0.0f;
            }
        };

        private Config m_config;

        private void ImportType_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void DoImport_CheckedChanged(object sender, EventArgs e)
        {
            bool enabled = (m_doImport.CheckState == CheckState.Checked);
            m_importType.Enabled = enabled;
            m_importSource.Enabled = enabled;
            m_importSourceBtn.Enabled = enabled;

            if (enabled)
            {
                m_createBlankTerrainCheck.CheckState = CheckState.Unchecked;
                m_config.Import = Config.ImportType.DEMFile;
            } 
            else
            {
                m_config.Import = 
                    (m_createBlankTerrainCheck.CheckState == CheckState.Checked) 
                    ? Config.ImportType.NewBlankTerrain : Config.ImportType.None;
            }
        }

        private void ImportSourceBtn_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog dlg = new OpenFileDialog())
            {
                dlg.CheckFileExists = true;
                if (dlg.ShowDialog(this) == DialogResult.OK)
                    m_importSource.Text = dlg.FileName;
            }
        }

        private void m_importSource_TextChanged(object sender, EventArgs e)
        {
            m_config.SourceDEMFile = m_importSource.Text;
            UpdateImportOp();
        }

        private void DoCreateBlank_CheckedChanged(object sender, EventArgs e)
        {
            bool enabled = (m_createBlankTerrainCheck.CheckState == CheckState.Checked);
            m_createCellsX.Enabled = enabled;
            m_createCellsY.Enabled = enabled;

            if (enabled)
            {
                m_doImport.CheckState = CheckState.Unchecked;
                m_config.Import = Config.ImportType.NewBlankTerrain;
            }
            else
            {
                m_config.Import =
                    (m_doImport.CheckState == CheckState.Checked)
                    ? Config.ImportType.DEMFile : Config.ImportType.None;
            }
        }

        private void m_createCellsX_TextChanged(object sender, EventArgs e)
        {
            uint newCellCount = 0;
            if (!uint.TryParse(m_createCellsX.Text, out newCellCount)) newCellCount = 0;
            m_config.NewCellCountX = newCellCount;
        }

        private void m_createCellsY_TextChanged(object sender, EventArgs e)
        {
            uint newCellCount = 0;
            if (!uint.TryParse(m_createCellsY.Text, out newCellCount)) newCellCount = 0;
            m_config.NewCellCountY = newCellCount;
        }

        private void UpdateImportOp()
        {
            m_config.ImportOp = null;
            if (m_config.Import == Config.ImportType.DEMFile && m_config.SourceDEMFile.Length > 0)
            {
                m_config.ImportOp = new GUILayer.TerrainImportOp(
                    m_config.SourceDEMFile,
                    m_config.NodeDimensions, m_config.CellTreeDepth);

                m_sourceHeightRangeMin.Text = m_config.ImportOp.ImportHeightRange.X.ToString();
                m_sourceHeightRangeMax.Text = m_config.ImportOp.ImportHeightRange.Y.ToString();
            }

            bool enableCtrls = m_config.ImportOp != null && m_config.ImportOp.SourceIsGood;
            m_sourceHeightRangeMin.Enabled = enableCtrls && !m_config.ImportOp.AbsoluteHeights;
            m_sourceHeightRangeMax.Enabled = enableCtrls && !m_config.ImportOp.AbsoluteHeights;
            m_importWarnings.Enabled = m_config.ImportOp != null && m_config.ImportOp.WarningCount > 0;
        }

        private void m_sourceHeightRangeMin_TextChanged(object sender, EventArgs e)
        {
            float newRangeMin = 0.0f;
            if (!float.TryParse(m_sourceHeightRangeMin.Text, out newRangeMin)) newRangeMin = 0;
            m_config.ImportOp.ImportHeightRange = 
                new GUILayer.Vector2(newRangeMin, m_config.ImportOp.ImportHeightRange.Y);
        }

        private void m_sourceHeightRangeMax_TextChanged(object sender, EventArgs e)
        {
            float newRangeMax = 1000.0f;
            if (!float.TryParse(m_sourceHeightRangeMax.Text, out newRangeMax)) newRangeMax = 0;
            m_config.ImportOp.ImportHeightRange =
                new GUILayer.Vector2(m_config.ImportOp.ImportHeightRange.X, newRangeMax);
        }

        private void m_importWarnings_Click(object sender, EventArgs e)
        {
            var builder = new StringBuilder();
            for (uint c = 0; c < m_config.ImportOp.WarningCount; ++c)
                builder.AppendLine(m_config.ImportOp.Warning(c));

            ControlsLibrary.BasicControls.TextWindow.ShowModal(builder.ToString());
        }
    }
}
