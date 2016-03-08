// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Windows.Forms;

namespace ControlsLibrary
{
    #if false
        public class LayerControlHack : IDisposable
        {
            public LayerControlHack(Control c) { }
            public void OnPaint(Control c, PaintEventArgs pe) { }
            public void OnPaintBackground(PaintEventArgs pe) { }
            public void OnResize(EventArgs pe) { }

            public void AddDefaultCameraHandler(GUILayer.VisCameraSettings settings) { }
            public void AddSystem(GUILayer.IOverlaySystem overlay) { }
            public void SetUpdateAsyncMan(bool updateAsyncMan) { }

            public void SetupDefaultVis(GUILayer.ModelVisSettings settings, GUILayer.VisMouseOver mouseOver, GUILayer.VisResources res) { }
            public GUILayer.VisMouseOver CreateVisMouseOver(GUILayer.ModelVisSettings settings, GUILayer.VisResources res) { return null; }
            public GUILayer.VisResources CreateVisResources() { return null; }

            public bool IsInputKey(Keys keyData) { return false; }
        
            public void Dispose() { }
            protected virtual void Dispose(bool disposing) { }
        }
    #else
        using LayerControlHack = GUILayer.LayerControl;
    #endif
    
    public partial class LayerControl : UserControl
    {
        public LayerControl()
        {
            InitializeComponent();

            layerControl = new LayerControlHack(this);
            ResizeRedraw = false;
            DoubleBuffered = false;
            SetStyle(ControlStyles.UserPaint, true);
            SetStyle(ControlStyles.AllPaintingInWmPaint, true);
            SetStyle(ControlStyles.Opaque, true);
        }

        public LayerControlHack Underlying { get { return layerControl; } }

        protected override void OnPaint(PaintEventArgs pe)
        {
            if (layerControl != null) layerControl.OnPaint(this, pe);
        }

        protected override bool IsInputKey(Keys keyData)
        {
            if (layerControl != null) return layerControl.IsInputKey(keyData);
            return false;
        }

        private LayerControlHack layerControl;
    }
}
