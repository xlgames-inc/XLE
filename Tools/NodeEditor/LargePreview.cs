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

namespace NodeEditor
{
    public partial class LargePreview : Form
    {
        public LargePreview(ShaderPatcherLayer.PreviewBuilder preview, ShaderPatcherLayer.NodeGraphContext doc)
        {
            _preview = preview;
            _document = doc;
            InitializeComponent();
            SetStyle(System.Windows.Forms.ControlStyles.AllPaintingInWmPaint, true);
            SetStyle(System.Windows.Forms.ControlStyles.UserPaint, true);
        }

        // protected override void OnPaint(PaintEventArgs e)
        // {
        //     base.OnPaint(e);
        //     if (e.Graphics == null)
        //         return;
        // 
        //        
        // }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            // suppress background paint entirely
        }

        private ShaderPatcherLayer.PreviewBuilder _preview;
        private ShaderPatcherLayer.NodeGraphContext _document;
        private Point _lastDragLocation;

        private void LargePreview_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                    // "capture" the mouse when we click, and start dragging the light direction
                Capture = true;
                _lastDragLocation = e.Location;
            }
        }

        private void LargePreview_MouseMove(object sender, MouseEventArgs e)
        {
            if (Capture)
            {
                // PreviewRender.Manager.Instance.RotateLightDirection(_document, new PointF(e.Location.X - _lastDragLocation.X, e.Location.Y - _lastDragLocation.Y));
                _lastDragLocation = e.Location;
                Invalidate();
            }
        }

        private void LargePreview_MouseUp(object sender, MouseEventArgs e)
        {
            Capture = false;
        }

        private void LargePreview_Paint(object sender, PaintEventArgs e)
        {
                //  create a bitmap from the preview renderer that will cover
                //  the entire surface we're rendering to. Then just draw that
                //  bitmap to the "Graphics"

            try
            {
                var bitmap = _preview.Build(
                    _document, new Size(ClientRectangle.Width, ClientRectangle.Height),
                    ShaderPatcherLayer.PreviewGeometry.Sphere, 0);
                if (bitmap != null)
                {
                    e.Graphics.DrawImage(bitmap, new Point() { X = 0, Y = 0 });
                }
            } catch {}
        }

        private void LargePreview_Resize(object sender, EventArgs e)
        {
                // invalidate everything on resize
            Invalidate();
        }

        private void LargePreview_Activated(object sender, EventArgs e)
        {
            Invalidate();
        }
    }
}
