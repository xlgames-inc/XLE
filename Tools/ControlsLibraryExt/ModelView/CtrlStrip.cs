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

namespace ControlsLibraryExt.ModelView
{
    public partial class CtrlStrip : UserControl
    {
        public CtrlStrip()
        {
            InitializeComponent();
            _colByMaterial.DataSource = Enum.GetValues(typeof(GUILayer.ModelVisSettings.ColourByMaterialType));
            _displayMode.DataSource = Enum.GetValues(typeof(DisplayMode));
        }

        private enum DisplayMode
        {
            Default,
            Wireframe,
            Normals,
            WireframeWithNormals
        };

        public GUILayer.ModelVisSettings Object;

        public event EventHandler OnChange;
        
        private void InvokeOnChange()
        {
            if (OnChange != null)
                OnChange.Invoke(this, EventArgs.Empty);
        }

        private void SelectModel(object sender, EventArgs e)
        {
            // We want to pop up a small drop-down window with a
            // property grid that can be used to set all of the properties
            // in the ModelViewSettings object. This is a handy non-model
            // way to change the selected model

            var dropDownCtrl = new PropertyGrid();
            dropDownCtrl.SelectedObject = Object;
            dropDownCtrl.ToolbarVisible = false;
            dropDownCtrl.HelpVisible = false;

            var toolDrop = new ToolStripDropDown();
            var toolHost = new ToolStripControlHost(dropDownCtrl);
            toolHost.Margin = new Padding(0);
            toolDrop.Padding = new Padding(0);
            toolDrop.Items.Add(toolHost);

            // we don't have a way to know the ideal height of the drop down ctrl... just make a guess based on the text height
            toolHost.AutoSize = false;
            toolHost.Size = new Size(512, 8 * dropDownCtrl.Font.Height);

            toolDrop.Show(this, PointToClient(MousePosition));
        }

        private void SelectColorByMaterial(object sender, EventArgs e)
        {
            if (Object == null) return;

            GUILayer.ModelVisSettings.ColourByMaterialType v;
            if (Enum.TryParse<GUILayer.ModelVisSettings.ColourByMaterialType>(((ComboBox)sender).SelectedValue.ToString(), out v))
            {
                Object.ColourByMaterial = v;
                InvokeOnChange();
            }
        }

        private void SelectDisplayMode(object sender, EventArgs e)
        {
            if (Object == null) return;

            DisplayMode v;
            if (Enum.TryParse<DisplayMode>(((ComboBox)sender).SelectedValue.ToString(), out v))
            {
                switch (v)
                {
                default:
                case DisplayMode.Default: Object.DrawNormals = Object.DrawWireframe = false; break;
                case DisplayMode.Wireframe: Object.DrawNormals = false;  Object.DrawWireframe = true; break;
                case DisplayMode.Normals: Object.DrawNormals = true; Object.DrawWireframe = false; break;
                case DisplayMode.WireframeWithNormals: Object.DrawNormals = true; Object.DrawWireframe = true; break;
                }
                InvokeOnChange();
            }
        }

        private void ResetCamClick(object sender, EventArgs e)
        {
            if (Object == null) return;
            Object.ResetCamera = true;
            InvokeOnChange();
        }
    }
}
