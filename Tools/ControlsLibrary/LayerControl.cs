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

namespace ControlsLibrary
{
    // internal class LayerControlHack : IDisposable
    // {
    //     public LayerControlHack(Control c) { }
    //     public void OnPaint(PaintEventArgs pe) { }
    //     public void OnPaintBackground(PaintEventArgs pe) { }
    //     public void OnResize(EventArgs pe) { }
    //     public void SetupDefaultVis(GUILayer.ModelVisSettings settings) { }
    //     public void AddDefaultCameraHandler(GUILayer.VisCameraSettings settings) { }
    //     public void AddSystem(GUILayer.IOverlaySystem overlay) { }
    //     public GUILayer.VisMouseOver CreateVisMouseOver(GUILayer.ModelVisSettings settings) { return null; }
    // 
    //     public void Dispose() { }
    //     protected virtual void Dispose(bool disposing) { }
    // }

    using LayerControlHack = GUILayer.LayerControl;
    
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
            if (layerControl != null) layerControl.OnPaint(pe);
        }

        protected override void OnResize(EventArgs pe)
        {
            if (layerControl!=null) layerControl.OnResize(pe);
            base.OnResize(pe);
        }

        protected override bool IsInputKey(Keys keyData)
        {
            if (layerControl != null) return layerControl.IsInputKey(keyData);
            return false;
        }

        private LayerControlHack layerControl;
    }
}
