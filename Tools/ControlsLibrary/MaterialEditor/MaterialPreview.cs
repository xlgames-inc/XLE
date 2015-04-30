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
            visLayer = new GUILayer.MaterialVisLayer(visSettings);
            preview.Underlying.AddSystem(visLayer);
            preview.Underlying.AddDefaultCameraHandler(visSettings.Camera);

            _geoType.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.GeometryType));
            _geoType.SelectedItem = visSettings.Geometry;
            _geoType.SelectedIndexChanged += ComboBoxSelectedIndexChanged;

            _lightingType.DataSource = Enum.GetValues(typeof(GUILayer.MaterialVisSettings.LightingType));
            _lightingType.SelectedItem = visSettings.Lighting;
            _lightingType.SelectedIndexChanged += ComboBoxSelectedIndexChanged;

            _environment.SelectedIndexChanged += SelectedEnvironmentChanged;
        }

        public IEnumerable<GUILayer.RawMaterial> Object
        {
            set 
            {
                string model = ""; ulong binding = 0;
                if (previewModel != null && previewModel.Item1 != null) { model = previewModel.Item1; binding = previewModel.Item2; }
                visLayer.SetConfig(value, model, binding);
                InvalidatePreview();
            }
        }

        public GUILayer.EnvironmentSettingsSet EnvironmentSet
        {
            set 
            {
                envSettings = value;
                _environment.DataSource = value.Names;
                _environment.Visible = true;
            }
        }

        public Tuple<string, ulong> PreviewModel
        {
            set { previewModel = value; }   // note -- this doesn't have an effect until the "Object" property is set
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
            InvalidatePreview();
        }

        private void SelectedEnvironmentChanged(object sender, System.EventArgs e)
        {
            visLayer.SetEnvironment(envSettings, _environment.SelectedValue.ToString());
            InvalidatePreview();
        }

        public void InvalidatePreview() { preview.Invalidate(); }

        protected GUILayer.MaterialVisLayer visLayer;
        protected GUILayer.MaterialVisSettings visSettings;
        protected GUILayer.EnvironmentSettingsSet envSettings;
        protected Tuple<string, ulong> previewModel = null;

        private void _resetCamera_Click(object sender, EventArgs e)
        {
            visSettings.ResetCamera = true;
            preview.Invalidate();
        }
    }
}
