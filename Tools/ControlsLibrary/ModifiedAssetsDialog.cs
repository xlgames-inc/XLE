// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Windows.Forms;

namespace ControlsLibrary
{
    public partial class ModifiedAssetsDialog : Form
    {
        public ModifiedAssetsDialog()
        {
            InitializeComponent();
            _action.DropDownItems.AddRange(
                new System.Collections.Generic.List<object> {
                    GUILayer.PendingSaveList.Action.Save,
                    GUILayer.PendingSaveList.Action.Abandon,
                    GUILayer.PendingSaveList.Action.Ignore
                });
            _assetList.LoadOnDemand = true;
        }

        public GUILayer.DivergentAssetList AssetList
        {
            set
            {
                _assetList.Model = value;
                _assetList.ExpandAll();
                _assetList.AutoSizeColumn(_treeColumn2);
            }
        }

        public void BuildAssetList(GUILayer.PendingSaveList pendingAssetList)
        {
            // some clients cannot construct "GUILayer.DivergentAssetList"
            // (because that requires adding a reference to the Aga.Controls library)
            // So this is provided for convenience to avoid that extra dependancy
            AssetList = new GUILayer.DivergentAssetList(
                GUILayer.EngineDevice.GetInstance(), pendingAssetList);
        }

        private void _tree_SelectionChanged(object sender, EventArgs e)
        {
            GUILayer.AssetItem selected = null;
            if (_assetList.SelectedNode != null)
                selected = _assetList.SelectedNode.Tag as GUILayer.AssetItem;
            if (selected != null && selected._pendingSave != null)
            {
                _compareWindow.Comparison = new Tuple<object, object>(
                    System.Text.Encoding.UTF8.GetString(selected._pendingSave._oldFileData),
                    System.Text.Encoding.UTF8.GetString(selected._pendingSave._newFileData));
            }
            else
            {
                _compareWindow.Comparison = null;
            }
        }

        private void _saveButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
        }

        private void _cancelButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
        }
    }

    public class PrettyNodeComboBox : Aga.Controls.Tree.NodeControls.NodeComboBox
    {
        protected override string FormatLabel(object obj)
        {
            return "<" + base.FormatLabel(obj) + ">";
        }
    }
}
