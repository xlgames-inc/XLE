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
    public partial class TerrainCoverageConfig : Form
    {
        public TerrainCoverageConfig()
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
                m_importSource.Text = value.SourceFile;

                UpdateImportOp();
            }
        }

        public class Config
        {
            [Category("General")] [Description("Resolution of the coverage layer (relative to heights resolution. 1 means the same as heights)")]
            public float Resolution { get; set; }

            [Category("General")] [Description("Enables or disables this layer")]
            public bool Enable { get; set; }

            [Category("General")] [Description("Id for this layer")]
            public uint Id { get; set; }

            [Category("General")] [Description("Normalization mode expected by the shader. 0: integer, 1: normalized, 2: float")]
            public uint ShaderNormalizationMode { get; set; }

            [Browsable(false)] public string SourceFile { get; set; }
            public enum ImportType { None, DEMFile, NewBlankTerrain };

            [Browsable(false)] public ImportType Import { get; set; }
            [Browsable(false)] public GUILayer.TerrainImportOp ImportOp { get; set; }

            [Browsable(false)] public uint NodeDimensions { get; set; }
            [Browsable(false)] public uint CellTreeDepth { get; set; }

            internal Config() 
            {
                Resolution = 2.0f;
                Enable = true;
                Id = 0;
                Import = ImportType.None;
                NodeDimensions = 32;
                CellTreeDepth = 5;
                ShaderNormalizationMode = 0;
            }
        };

        private Config m_config;

        private void ImportType_SelectedIndexChanged(object sender, EventArgs e) {}

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
            m_config.SourceFile = m_importSource.Text;
            UpdateImportOp();
        }

        private void DoCreateBlank_CheckedChanged(object sender, EventArgs e)
        {
            bool enabled = (m_createBlankTerrainCheck.CheckState == CheckState.Checked);
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

        private void UpdateImportOp()
        {
            m_config.ImportOp = null;
            if (m_config.Import == Config.ImportType.DEMFile && m_config.SourceFile.Length > 0)
            {
                m_config.ImportOp = new GUILayer.TerrainImportOp(
                    m_config.SourceFile,
                    m_config.NodeDimensions, m_config.CellTreeDepth);
                m_importFormat.Text = m_config.ImportOp.ImportCoverageFormat.ToString();
            }

            bool enableCtrls = m_config.ImportOp != null && m_config.ImportOp.SourceIsGood;
            m_importWarnings.Enabled = m_config.ImportOp != null && m_config.ImportOp.WarningCount > 0;
            m_importFormat.Enabled = enableCtrls;
        }

        private void m_importFormat_TextChanged(object sender, EventArgs e)
        {
            uint newValue = 0;
            if (!uint.TryParse(m_importFormat.Text, out newValue)) newValue = 0;
            m_config.ImportOp.ImportCoverageFormat = newValue;
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
