// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Drawing;
using System.Windows.Forms;

namespace ControlsLibraryExt.ModelView
{
    public partial class CtrlStrip : UserControl
    {
        public CtrlStrip()
        {
            InitializeComponent();
            _colByMaterial.DataSource = Enum.GetValues(typeof(GUILayer.VisOverlaySettings.ColourByMaterialType));
            _displayMode.DataSource = Enum.GetValues(typeof(DisplayMode));
            _skeletonMode.DataSource = Enum.GetValues(typeof(SkeletonMode));
        }

        private enum DisplayMode
        {
            Default,
            Wireframe,
            Normals,
            WireframeWithNormals
        };

        private enum SkeletonMode
        {
            NoSkeleton,
            Skeleton,
            BoneNames
        };

        public GUILayer.VisOverlaySettings OverlaySettings;
        public GUILayer.ModelVisSettings ModelSettings;

        public event EventHandler OverlaySettings_OnChange;
        public event EventHandler ModelSettings_OnChange;
        public event EventHandler OnResetCamera;

        private void OverlaySettings_InvokeOnChange()
        {
            if (OverlaySettings_OnChange != null)
                OverlaySettings_OnChange.Invoke(this, EventArgs.Empty);
        }

        private void ModelSettings_InvokeOnChange()
        {
            if (ModelSettings_OnChange != null)
                ModelSettings_OnChange.Invoke(this, EventArgs.Empty);
        }

        private void SelectModel(object sender, EventArgs e)
        {
            // We want to pop up a small drop-down window with a
            // property grid that can be used to set all of the properties
            // in the ModelViewSettings object. This is a handy non-model
            // way to change the selected model

            var dropDownCtrl = new PropertyGrid();
            dropDownCtrl.SelectedObject = ModelSettings;
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
            toolDrop.Closing += ToolDrop_Closing;
        }

        private void ToolDrop_Closing(object sender, ToolStripDropDownClosingEventArgs e)
        {
            ModelSettings_InvokeOnChange();
        }

        private void SelectColorByMaterial(object sender, EventArgs e)
        {
            if (OverlaySettings == null) return;

            GUILayer.VisOverlaySettings.ColourByMaterialType v;
            if (Enum.TryParse(((ComboBox)sender).SelectedValue.ToString(), out v))
            {
                OverlaySettings.ColourByMaterial = v;
                OverlaySettings_InvokeOnChange();
            }
        }

        private void SelectDisplayMode(object sender, EventArgs e)
        {
            if (OverlaySettings == null) return;

            DisplayMode v;
            if (Enum.TryParse(((ComboBox)sender).SelectedValue.ToString(), out v))
            {
                switch (v)
                {
                default:
                case DisplayMode.Default: OverlaySettings.DrawNormals = OverlaySettings.DrawWireframe = false; break;
                case DisplayMode.Wireframe: OverlaySettings.DrawNormals = false; OverlaySettings.DrawWireframe = true; break;
                case DisplayMode.Normals: OverlaySettings.DrawNormals = true; OverlaySettings.DrawWireframe = false; break;
                case DisplayMode.WireframeWithNormals: OverlaySettings.DrawNormals = true; OverlaySettings.DrawWireframe = true; break;
                }
                OverlaySettings_InvokeOnChange();
            }
        }

        private void SelectSkeletonMode(object sender, EventArgs e)
        {
            if (OverlaySettings == null) return;

            SkeletonMode v;
            if (Enum.TryParse(((ComboBox)sender).SelectedValue.ToString(), out v))
            {
                switch (v)
                {
                    default:
                    case SkeletonMode.NoSkeleton: OverlaySettings.SkeletonMode = GUILayer.VisOverlaySettings.SkeletonModes.None; break;
                    case SkeletonMode.Skeleton: OverlaySettings.SkeletonMode = GUILayer.VisOverlaySettings.SkeletonModes.Render; break;
                    case SkeletonMode.BoneNames: OverlaySettings.SkeletonMode = GUILayer.VisOverlaySettings.SkeletonModes.BoneNames; break;
                }
                OverlaySettings_InvokeOnChange();
            }
        }

        private void ResetCamClick(object sender, EventArgs e)
        {
            if (OnResetCamera != null)
                OnResetCamera.Invoke(this, null);
        }
    }
}
