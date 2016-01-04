// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf.Controls;
using Sce.Atf.Controls.Adaptable;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Collections.Generic;
using System;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace MaterialTool
{
    interface IDiagramControl
    {
        void SetContext(DiagramEditingContext context);
    }

    [Export(typeof(IDiagramControl))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    class DiagramControl : AdaptableControl, IDiagramControl
    {
        public DiagramControl()
        {
            this.AllowDrop = true;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(70)))), ((int)(((byte)(70)))), ((int)(((byte)(70)))));
            Paint += child_Paint;
        }

        public void SetContext(DiagramEditingContext context)
        {
            Context = context;

            // We also setup an underlying context that will be used
            // while rendering node items.
            var underlyingDoc = _exportProvider.GetExport<NodeEditorCore.DiagramDocument>().Value;
            underlyingDoc.ViewModel = context.Model;
            underlyingDoc.ParameterSettings = 
                new ShaderPatcherLayer.Document 
                    { DefaultsMaterial = GUILayer.RawMaterial.Get(_activeMaterialContext.MaterialName) };

            var graphAdapter = new HyperGraphAdapter();
            graphAdapter.HighlightCompatible = true;
            graphAdapter.LargeGridStep = 160F;
            graphAdapter.SmallGridStep = 20F;
            graphAdapter.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(90)))), ((int)(((byte)(90)))), ((int)(((byte)(90)))));
            graphAdapter.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(80)))), ((int)(((byte)(80)))), ((int)(((byte)(80)))));
            graphAdapter.ShowLabels = false;
            graphAdapter.Model = context.Model;
            graphAdapter.Selection = context.DiagramSelection; 
            graphAdapter.Context = underlyingDoc;

            // calling Adapt will unbind previous adapters
            var hoverAdapter = new HoverAdapter();
            hoverAdapter.HoverStarted += (object sender, HoverEventArgs<object, object> args) =>
                {
                    if (_hover != null) return;

                    var n = args.Object as HyperGraph.Node;
                    if (n == null)
                    {
                        var i = args.Object as HyperGraph.NodeItem;
                        if (i != null) n = i.Node;
                    }
                    if (n != null)
                    {
                        _hover = new HoverLabel(n.Title)
                        {
                            Location = new Point(MousePosition.X - 8, MousePosition.Y + 8)
                        };
                        _hover.ShowWithoutFocus();
                    }
                };
            hoverAdapter.HoverStopped += EndHover;
            MouseLeave += EndHover;
            Adapt(new IControlAdapter[] { graphAdapter, new PickingAdapter { Context = context }, hoverAdapter });
        }

        private void EndHover(object sender, EventArgs args)
        {
            if (_hover == null) return;
            _hover.Hide();
            _hover.Dispose();
            _hover = null;
        }

        void child_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
        {
            _engine.ForegroundUpdate();
        }

        [Import] private GUILayer.EngineDevice _engine;
        [Import] private System.ComponentModel.Composition.Hosting.ExportProvider _exportProvider;
        [Import] private ControlsLibraryExt.Material.ActiveMaterialContext _activeMaterialContext;
        private HoverLabel _hover = null;
    }

    class HyperGraphAdapter : NodeEditorCore.GraphControl, IControlAdapter
    {
        public AdaptableControl AdaptedControl { get; set; }

        public void Bind(AdaptableControl control)
        {
            Unbind(AdaptedControl);
            AdaptedControl = control;
            Attach(control);
        }
        public void BindReverse(AdaptableControl control) { }
        public void Unbind(AdaptableControl control)
        {
            if (control == null) return;
            Detach(control);
        }
    }

    class PickingAdapter : ControlAdapter, IPickingAdapter2
    {
        internal DiagramEditingContext Context;

        public DiagramHitRecord Pick(Point p)
        {
            return new DiagramHitRecord(HyperGraph.GraphControl.FindElementAt(Context.Model, p));
        }

        public IEnumerable<object> Pick(Rectangle bounds)
        {
            RectangleF rectF = new RectangleF((float)bounds.X, (float)bounds.Y, (float)bounds.Width, (float)bounds.Height);
            return HyperGraph.GraphControl.RectangleSelection(Context.Model, rectF);
        }

        public Rectangle GetBounds(IEnumerable<object> items)
        {
            float minX = float.MaxValue, minY = float.MaxValue;
            float maxX = -float.MaxValue, maxY = -float.MaxValue;

            foreach (var o in items)
            {
                var n = o as HyperGraph.Node;
                if (n != null)
                {
                    minX = System.Math.Min(minX, n.bounds.Left);
                    minY = System.Math.Min(minY, n.bounds.Top);
                    maxX = System.Math.Max(maxX, n.bounds.Right);
                    maxY = System.Math.Max(maxY, n.bounds.Bottom);
                }
            }

            return new Rectangle((int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY));
        }
    }
}
