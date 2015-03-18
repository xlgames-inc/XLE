// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ModelViewer.Controls
{
    public partial class MaterialPreview : UserControl
    {
        public MaterialPreview()
        {
            InitializeComponent();
            visSettings = GUILayer.MaterialVisSettings.CreateDefault();

            comboBox1.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.GeometryType));
            comboBox1.SelectedItem = visSettings.Geometry;
            comboBox1.SelectedIndexChanged += ComboBoxSelectedIndexChanged;

            comboBox2.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.LightingType));
            comboBox2.SelectedItem = visSettings.Lighting;
            comboBox2.SelectedIndexChanged += ComboBoxSelectedIndexChanged;
        }

        public GUILayer.RawMaterial Object
        {
            set 
            {
                if (visLayer == null) {
                    visLayer = new GUILayer.MaterialVisLayer(visSettings, value);
                    preview.Underlying.AddSystem(visLayer);
                    preview.Underlying.AddDefaultCameraHandler(visSettings.Camera);
                } else {
                    visLayer.SetConfig(value);
                }
            }
        }

        private void ComboBoxSelectedIndexChanged(object sender, System.EventArgs e)
        {
            GUILayer.MaterialVisSettings.GeometryType newGeometry;
            if (Enum.TryParse<GUILayer.MaterialVisSettings.GeometryType>(comboBox1.SelectedValue.ToString(), out newGeometry))
            {
                visSettings.Geometry = newGeometry;
            }

            GUILayer.MaterialVisSettings.LightingType newLighting;
            if (Enum.TryParse<GUILayer.MaterialVisSettings.LightingType>(comboBox2.SelectedValue.ToString(), out newLighting))
            {
                visSettings.Lighting = newLighting;
            }
        }

        protected GUILayer.MaterialVisLayer visLayer;
        protected GUILayer.MaterialVisSettings visSettings;
    }
}
