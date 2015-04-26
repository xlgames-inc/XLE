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

namespace ControlsLibrary.MaterialEditor
{
    public partial class MaterialPreview : UserControl
    {
        public MaterialPreview()
        {
            InitializeComponent();
            visSettings = GUILayer.MaterialVisSettings.CreateDefault();

            _geoType.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.GeometryType));
            _geoType.SelectedItem = visSettings.Geometry;
            _geoType.SelectedIndexChanged += ComboBoxSelectedIndexChanged;

            _lightingType.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.LightingType));
            _lightingType.SelectedItem = visSettings.Lighting;
            _lightingType.SelectedIndexChanged += ComboBoxSelectedIndexChanged;
        }

        public IEnumerable<GUILayer.RawMaterial> Object
        {
            set 
            {
                string model = "", binding = "";
                if (previewModel != null) { model = previewModel.Item1; binding = previewModel.Item2; }

                if (visLayer == null) {
                    visLayer = new GUILayer.MaterialVisLayer(visSettings, value, model, binding);
                    preview.Underlying.AddSystem(visLayer);
                    preview.Underlying.AddDefaultCameraHandler(visSettings.Camera);
                } else {
                    visLayer.SetConfig(value, model, binding);
                }
            }
        }

        public Tuple<string, string> PreviewModel
        {
            set { previewModel = value; }
        }

        private void ComboBoxSelectedIndexChanged(object sender, System.EventArgs e)
        {
            GUILayer.MaterialVisSettings.GeometryType newGeometry;
            if (Enum.TryParse<GUILayer.MaterialVisSettings.GeometryType>(_geoType.SelectedValue.ToString(), out newGeometry))
            {
                visSettings.Geometry = newGeometry;
            }

            GUILayer.MaterialVisSettings.LightingType newLighting;
            if (Enum.TryParse<GUILayer.MaterialVisSettings.LightingType>(_lightingType.SelectedValue.ToString(), out newLighting))
            {
                visSettings.Lighting = newLighting;
            }
            preview.Invalidate();
        }

        protected GUILayer.MaterialVisLayer visLayer;
        protected GUILayer.MaterialVisSettings visSettings;
        protected Tuple<string, string> previewModel = null;

        private void _resetCamera_Click(object sender, EventArgs e)
        {
            visSettings.ResetCamera = true;
            preview.Invalidate();
        }
    }
}
