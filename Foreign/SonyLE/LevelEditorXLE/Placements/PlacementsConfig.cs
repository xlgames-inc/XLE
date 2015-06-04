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

namespace LevelEditorXLE.Placements
{
    public partial class PlacementsConfig : Form
    {
        public PlacementsConfig()
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
            [Category("Basic")] [Description("Cell Count (X)")]
            public uint CellCountX { get; set; }

            [Category("Basic")] [Description("Cell Count (Y)")]
            public uint CellCountY { get; set; }

            [Category("Basic")] [Description("Cells Origin (X) in metres")]
            public float CellsOriginX { get; set; }

            [Category("Basic")] [Description("Cellx Origin (Y) in metres")]
            public float CellsOriginY { get; set; }

            [Category("Basic")] [Description("Cell Size (in metres)")]
            public float CellSize { get; set; }

            [Category("Basic")] [Description("Base Level Editor Path")]
            public string BaseEditorPath { get; set; }

            [Category("Basic")] [Description("Base Export Path")]
            public string BaseExportPath { get; set; }

            internal Config() { CellCountX = 4; CellCountY = 4; CellSize = 512; }
        };

        private Config m_config;
    }
}
