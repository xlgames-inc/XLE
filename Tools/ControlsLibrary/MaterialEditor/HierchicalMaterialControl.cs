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

        public string Object
        {
            set
            {
                    // Build a hierarchical of nodes in the combo box
                    // representing the inheritance tree in the material object
                ClearComboBoxNodes();
                if (value!=null) {
                    ComboTreeNode firstNode = null;

                    var split = value.Split(';').ToList();
                    for (int c = 0; c < split.Count; ++c)
                        firstNode = AddNode(split.GetRange(0, c+1));

                    _hierachyTree.SelectedNodeChanged += SubMatSelectedNodeChanged; 
                    _hierachyTree.SelectedNode = firstNode;
                }
            }
        }

        public Tuple<string, ulong> PreviewModel
        {
            set { _materialControl.PreviewModel = value; }
        }

        public GUILayer.EnvironmentSettingsSet EnvironmentSet
        {
            set { _materialControl.EnvironmentSet = value; }
        }

        public MaterialControl FocusedMatControls { get { return _materialControl; } }

        protected void SubMatSelectedNodeChanged(object sender, EventArgs e)
        {
                //  When the selected node changes, we want to 
                //  change the object that we're currently editing...
            var mat = (_hierachyTree.SelectedNode!=null) ? (_hierachyTree.SelectedNode.Tag as IEnumerable<string>) : null;
            _materialControl.Object = mat;
        }

        protected void ClearComboBoxNodes()
        {
            _hierachyTree.Nodes.Clear();
        }

        protected ComboTreeNode AddNode(IList<string> names)
        {
            string topItem = names[names.Count - 1];
            var parentNode = _hierachyTree.Nodes.Add(topItem);
            parentNode.Tag = names;
            AddComboBoxChildren(parentNode, GUILayer.RawMaterial.BuildInheritanceList(topItem));
            return parentNode;
        }

        protected void AddComboBoxChildren(
            ComboTreeNode parentNode, 
            List<string> childMats)
        {
            if (childMats == null) return;
            foreach (var mat in childMats)
            {
                var newNode = parentNode.Nodes.Add(mat);
                newNode.Tag = new List<string>() { mat };
                AddComboBoxChildren(newNode, GUILayer.RawMaterial.BuildInheritanceList(mat));
            }
        }
    }
}
