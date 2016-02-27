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
            visResources = viewerControl.Underlying.CreateVisResources();
            visMouseOver = viewerControl.Underlying.CreateVisMouseOver(visSettings, visResources);
            viewerControl.Underlying.SetupDefaultVis(visSettings, visMouseOver, visResources);

            viewSettings.SelectedObject = visSettings;
            visSettings.AttachCallback(mouseOverDetails);

            mouseOverDetails.SelectedObject = visMouseOver;
            visMouseOver.AttachCallback(mouseOverDetails);

            viewerControl.MouseClick += OnViewerMouseClick;
            viewerControl.Underlying.SetUpdateAsyncMan(true);
        }

        protected void ContextMenu_EditMaterial(object sender, EventArgs e)
        {
            if (visMouseOver.HasMouseOver) 
            {
                    // pop-up a modal version of the material editor (for testing/prototyping)
                if (visMouseOver.FullMaterialName != null)
                {
                    using (var editor = new ModalMaterialEditor()) {
                        editor.PreviewModel = Tuple.Create(visMouseOver.ModelName, visMouseOver.MaterialBindingGuid); 
                        editor.Object = visMouseOver.FullMaterialName;
                        editor.ShowDialog();
                    }
                }
            }
        }

        protected void ContextMenu_ShowModifications(object sender, EventArgs e)
        {
            var pendingAssetList = GUILayer.PendingSaveList.Create();
            var assetList = new GUILayer.DivergentAssetList(GUILayer.EngineDevice.GetInstance(), pendingAssetList);

            using (var dialog = new ControlsLibrary.ModifiedAssetsDialog()) 
            {
                dialog.AssetList = assetList;
                var result = dialog.ShowDialog();

                if (result == DialogResult.OK)
                {
                    var cmtResult = pendingAssetList.Commit();

                    // if we got some error messages during the commit; display them here...
                    if (!String.IsNullOrEmpty(cmtResult.ErrorMessages))
                        ControlsLibrary.BasicControls.TextWindow.ShowModal(cmtResult.ErrorMessages);
                }
            }
        }

        protected void ContextMenu_ShowInvalidAssets(object sender, EventArgs e)
        {
            using (var dialog = new ControlsLibrary.InvalidAssetDialog())
                dialog.ShowDialog();
        }

        protected void OnViewerMouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right) {
                ContextMenu cm = new ContextMenu();
                cm.MenuItems.Add("Show Modifications", new EventHandler(ContextMenu_ShowModifications));
                cm.MenuItems.Add("Show Invalid Assets", new EventHandler(ContextMenu_ShowInvalidAssets));
                if (visMouseOver.HasMouseOver)
                    cm.MenuItems.Add("Edit &Material", new EventHandler(ContextMenu_EditMaterial));
                cm.Show(this, e.Location);
            }
        }

        private GUILayer.ModelVisSettings visSettings;
        private GUILayer.VisMouseOver visMouseOver;
        private GUILayer.VisResources visResources;
    }
}
