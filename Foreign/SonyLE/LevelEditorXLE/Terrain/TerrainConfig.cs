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
                var props = TypeDescriptor.GetProperties(typeof(Config));
                m_propertyGrid1.Bind(
                    new XLEBridgeUtils.BasicPropertyEditingContext(m_config, props));
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

            [Category("Files")] [Description("Source DEM file")]
            [EditorAttribute(typeof(GUILayer.FileNameEditor), typeof(System.Drawing.Design.UITypeEditor))]
            public string SourceDEMFile { get; set; }

            [Category("Files")] [Description("UberSurface directory")]
            public string UberSurfaceDirectory { get; set; }

            [Category("Files")] [Description("Cells directory")]
            public string CellsDirectory { get; set; }

            internal Config() { NodeDimensions = 32; Overlap = 2; Spacing = 10; }
        };

        private Config m_config;
    }
}
