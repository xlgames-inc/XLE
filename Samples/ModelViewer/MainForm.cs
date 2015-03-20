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
            visMouseOver = viewerControl.Underlying.CreateVisMouseOver(visSettings);
            viewerControl.Underlying.SetupDefaultVis(visSettings, visMouseOver);

            viewSettings.SelectedObject = visSettings;
            visSettings.AttachCallback(mouseOverDetails);

            mouseOverDetails.SelectedObject = visMouseOver;
            visMouseOver.AttachCallback(mouseOverDetails);

            viewerControl.MouseClick += OnViewerMouseClick;
            
                // pop-up a modal version of the material editor (for testing/prototyping)
            // using (var editor = new ModalMaterialEditor())
            // {
            //     editor.Object = new GUILayer.RawMaterial(
            //         "Game\\Model\\Galleon\\galleon.dae:galleon_01");
            //     editor.ShowDialog();
            // }
        }

        protected void ContextMenu_EditMaterial(object sender, EventArgs e)
        {
            if (visMouseOver.HasMouseOver) {
                    // pop-up a modal version of the material editor (for testing/prototyping)
                var matName = visMouseOver.FullMaterialName;
                if (matName != null) {
                    using (var editor = new ModalMaterialEditor()) {
                        editor.Object = new GUILayer.RawMaterial(matName);
                        editor.ShowDialog();
                    }
                }
            }
        }

        protected void OnViewerMouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right) {
                if (visMouseOver.HasMouseOver) {
                    ContextMenu cm = new ContextMenu();
                    cm.MenuItems.Add("Edit &Material", new EventHandler(ContextMenu_EditMaterial));
                    cm.Show(this, e.Location);
                }
            }
        }

        private GUILayer.ModelVisSettings visSettings;
        private GUILayer.VisMouseOver visMouseOver;
    }
}
