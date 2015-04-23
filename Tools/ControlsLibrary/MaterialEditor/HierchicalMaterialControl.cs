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
    public partial class HierchicalMaterialControl : UserControl
    {
        public HierchicalMaterialControl()
        {
            InitializeComponent();
        }

        public GUILayer.RawMaterial Object
        {
            set
            {
                materialControl1.Object = value;

                    // Build a hierarchical of nodes in the combo box
                    // representing the inheritance tree in the material object
                ClearComboBoxNodes();
                if (value!=null) {
                    var parentNode = treeView1.Nodes.Add(
                        value.Filename + ":" + value.SettingName);
                    parentNode.Tag = new GUILayer.RawMaterial(value);
                    AddComboBoxChildren(parentNode, value.BuildInheritanceList());
                    parentNode.Expanded = true;
                    treeView1.SelectedNode = parentNode;

                    treeView1.SelectedNodeChanged += SubMatSelectedNodeChanged;
                }
            }
        }

        protected void SubMatSelectedNodeChanged(object sender, EventArgs e)
        {
                //  When the selected node changes, we want to 
                //  change the object that we're currently editing...
            var mat = (treeView1.SelectedNode!=null) ? (treeView1.SelectedNode.Tag as GUILayer.RawMaterial) : null;
            materialControl1.Object = mat;
        }

        protected void ClearComboBoxNodes()
        {
                //  hack to explicitly dispose all of the RawMaterial objects that have
                //  been attached to 
            foreach(var node in treeView1.AllNodes)
            {
                var mat = node.Tag as GUILayer.RawMaterial;
                if (mat != null)
                    mat.Dispose();
            }
            treeView1.Nodes.Clear();
        }

        protected void AddComboBoxChildren(
            ComboTreeNode parentNode, 
            List<GUILayer.RawMaterial> childMats)
        {
            if (childMats == null) return;
            foreach (var mat in childMats)
            {
                var newNode = parentNode.Nodes.Add(mat.Filename + ":" + mat.SettingName);
                newNode.Tag = mat;
                AddComboBoxChildren(newNode, mat.BuildInheritanceList());
            }
        }
    }
}
