using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Drawing;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;

using ControlsLibrary.MaterialEditor;

namespace MaterialTool.AdaptiveNodes
{
    [Export(typeof(MaterialControl.ExtraControls))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    class MaterialTab : MaterialControl.ExtraControls
    {
        public MaterialTab()
        {
            BackColor = Color.Black;

            SuspendLayout();
            Dock = DockStyle.Fill;
            Padding = new Padding(0);
            ResumeLayout(false);

            _guiFrame.OnInvalidate += _guiFrame_OnInvalidate;
        }

        private void _guiFrame_OnInvalidate(object sender, EventArgs e)
        {
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            if (_guiFrame.Storage != null)
            {
                _guiFrame.Draw(e.Graphics, this.ClientRectangle, null);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (_guiFrame.Storage != null)
            {
                _guiFrame.OnMouseMove(e);
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (_guiFrame.Storage != null)
            {
                _guiFrame.OnMouseDown(e);
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            if (_guiFrame.Storage != null)
            {
                _guiFrame.OnMouseUp(e);
            }
            base.OnMouseUp(e);
        }

        protected override void OnResize(EventArgs args)
        {
            if (_guiFrame.Storage != null)
            {
                _guiFrame.InvalidateLayout();
                Invalidate();
            }
            base.OnResize(args);
        }

        public override GUILayer.RawMaterial Object
        {
            set
            {
                if (value != null)
                {
                    _guiFrame.Storage = new RawMaterialStorage { Material = value };
                }
                else
                {
                    _guiFrame.Storage = null;
                }
            }
        }

        public override string TabName { get { return "Adaptive"; } }

        // public IDataBlock Storage { set { _guiFrame.Storage = value; } }
        // public IDataBlockDeclaration Declaration { set { _guiFrame.Declaration = value; } }

        private AuthoringConcept.AdaptiveEditing.EditorFrame _guiFrame = new AuthoringConcept.AdaptiveEditing.EditorFrame();
    }
}
