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

namespace ModelViewer
{
    public partial class MainForm : Form
    {
        public MainForm()
        {
            InitializeComponent();

            visSettings = GUILayer.ModelVisSettings.CreateDefault();
            viewerControl.SetupDefaultVis(visSettings);
            propertyGrid1.SelectedObject = visSettings;
            visSettings.AttachCallback(propertyGrid1);

                // pop-up a modal version of the material editor (for testing/prototyping)
            using (var editor = new ModalMaterialEditor())
            {
                editor.Object = new GUILayer.RawMaterial(
                    // "Game\\Model\\Galleon\\galleon.material:galleon_sail");
                    // "int\\d0\\game\\model\\galleon\\galleon.dae-rawmat:93405fbac7096378");
                    "int\\d0\\game\\model\\galleon\\galleon.dae-rawmat:742a2bffd0e885af");
                editor.ShowDialog();
            }
        }

        private GUILayer.ModelVisSettings visSettings;
    }
}
