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
    public partial class LayerControl : UserControl
    {
        public LayerControl()
        {
            InitializeComponent();

            layerControl = new GUILayer.LayerControl(this);
        }

        public GUILayer.LayerControl Underlying { get { return layerControl; } }

        protected override void OnPaint(PaintEventArgs pe)
        {
            if (layerControl != null) layerControl.OnPaint(pe);
        }

        protected override void OnPaintBackground(PaintEventArgs pe)
        {
            if (layerControl != null) layerControl.OnPaintBackground(pe);
        }

        protected override void OnResize(EventArgs pe)
        {
            if (layerControl!=null) layerControl.OnResize(pe);
            base.OnResize(pe);
        }

        protected GUILayer.LayerControl layerControl;
    }
}
