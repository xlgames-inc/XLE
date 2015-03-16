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
        }

        public GUILayer.RawMaterialConfiguration Object
        {
            set 
            {
                if (visLayer == null) {
                    visLayer = new GUILayer.MaterialVisLayer(visSettings, value);
                    preview.AddSystem(visLayer);
                } else {
                    visLayer.SetConfig(value);
                }
            }
        }

        protected GUILayer.MaterialVisLayer visLayer;
        protected GUILayer.MaterialVisSettings visSettings;
    }
}
