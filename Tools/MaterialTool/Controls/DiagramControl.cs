// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf.Controls;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Applications;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Collections.Generic;
using System;
using Sce.Atf;
using System.Windows.Forms;
using Sce.Atf.Adaptation;
using System.Drawing.Drawing2D;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace MaterialTool.Controls
{
    interface IDiagramControl
    {
        void SetContext(DiagramEditingContext context);
    }

    class AdaptableSet : IAdaptable, IDecoratable
    {
        public object GetAdapter(Type type)
        {
            foreach(var o in _subObjects)
            {
                var r = o.As(type);
                if (r != null) return r;
            }
            return null;
        }

        public IEnumerable<object> GetDecorators(Type type)
        {
            foreach (var o in _subObjects)
            {
                var d = o.As<IDecoratable>();
                if (d==null) continue;
                var l = d.GetDecorators(type);
                foreach (var i in l) yield return i;
            }
        }

        public AdaptableSet(IEnumerable<object> subObjects) { _subObjects = subObjects; }

        private IEnumerable<object> _subObjects;
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
            var existingContext = context as ISelectionContext;
            if (existingContext != null)
                existingContext.SelectionChanged -= Context_SelectionChanged;

            var graphAdapter = new HyperGraphAdapter();
            graphAdapter.HighlightCompatible = true;
            graphAdapter.LargeGridStep = 160F;
            graphAdapter.SmallGridStep = 20F;
            graphAdapter.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(90)))), ((int)(((byte)(90)))), ((int)(((byte)(90)))));
            graphAdapter.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(80)))), ((int)(((byte)(80)))), ((int)(((byte)(80)))));
            graphAdapter.ShowLabels = false;
            graphAdapter.Model = context.Model;
            graphAdapter.Selection = context.DiagramSelection; 
            graphAdapter.Context = context.UnderlyingDocument;
            graphAdapter.ModelConversion = _modelConversion;
            graphAdapter.NodeFactory = _nodeFactory;
            graphAdapter.Document = context.UnderlyingDocument;

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
            Adapt(
                new IControlAdapter[] {
                    graphAdapter,
                    new PickingAdapter { Context = context },
                    new CanvasAdapter(),
                    new ViewingAdapter(graphAdapter),
                    hoverAdapter
                });
            context.SelectionChanged += Context_SelectionChanged;

            // Our context is actually a collection of 2 separate context objects
            // - What represents the model itself
            // - Another is the "ViewingContext", which is how we're looking at the model
            // Curiously, there seems to be a bit of an overlap between control adapters
            // and the viewing context. For example, ViewingContext, PickingAdapter and ViewingAdapter
            // duplicate some of the same functionality.
            // However, all of these are needed to use the standard ATF commands for framing and aligning
            _contextSet = new AdaptableSet(new object[]{ context, new ViewingContext { Control = this } });
            Context = _contextSet;
        }

        private void Context_SelectionChanged(object sender, EventArgs e)
        {
            Invalidate();
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
            GUILayer.EngineDevice.GetInstance().ForegroundUpdate();
        }

        [Import] private System.ComponentModel.Composition.Hosting.ExportProvider _exportProvider;
        [Import] private ControlsLibraryExt.Material.ActiveMaterialContext _activeMaterialContext;
        [Import] private NodeEditorCore.IModelConversion _modelConversion;
        [Import] private NodeEditorCore.IShaderFragmentNodeCreator _nodeFactory;
        private HoverLabel _hover = null;
        private AdaptableSet _contextSet;
    }

    class HyperGraphAdapter : NodeEditorCore.GraphControl, IControlAdapter, ITransformAdapter
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

        #region ITransformAdapter
        Matrix ITransformAdapter.Transform
        {
            get { return base.Transform; }
        }

        public PointF Translation
        {
            get { return new PointF(Transform.OffsetX, Transform.OffsetY); }
            set { SetTranslation(value); }
        }

        public PointF Scale
        {
            get
            {
                float[] m = Transform.Elements;
                return new PointF(m[0], m[3]);
            }
            set
            {
                SetScale(value);
            }
        }

        public bool EnforceConstraints
        {
            set { _enforceConstraints = value; }
            get { return _enforceConstraints; }
        }

        public PointF MinTranslation
        {
            get { return _minTranslation; }
            set
            {
                _minTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MaxTranslation
        {
            get { return _maxTranslation; }
            set
            {
                _maxTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MinScale
        {
            get { return _minScale; }
            set
            {
                if (value.X <= 0 ||
                    value.X > _maxScale.X ||
                    value.Y <= 0 ||
                    value.Y > _maxScale.Y)
                {
                    throw new ArgumentException("minimum components must be > 0 and less than maximum");
                }

                _minScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public PointF MaxScale
        {
            get { return _maxScale; }
            set
            {
                if (value.X < _minScale.X ||
                    value.Y < _minScale.Y)
                {
                    throw new ArgumentException("maximum components must be greater than minimum");
                }

                _maxScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public bool UniformScale
        {
            get { return _uniformScale; }
            set { _uniformScale = value; }
        }

        public void SetTransform(float xScale, float yScale, float xTranslation, float yTranslation)
        {
            PointF scale = EnforceConstraints ? this.ConstrainScale(new PointF(xScale, yScale)) : new PointF(xScale, yScale);
            PointF translation = EnforceConstraints ? this.ConstrainTranslation(new PointF(xTranslation, yTranslation)) : new PointF(xTranslation, yTranslation);
            SetTransformInternal((scale.X + scale.Y) * 0.5f, translation.X, translation.Y);
        }

        private void SetTranslation(PointF translation)
        {
            translation = EnforceConstraints ? this.ConstrainTranslation(translation) : translation;
            SetTransformInternal(_zoom, translation.X, translation.Y);
        }

        private void SetScale(PointF scale)
        {
            scale = EnforceConstraints ? this.ConstrainScale(scale) : scale;
            SetTransformInternal((scale.X + scale.Y) * 0.5f, _translation.X, _translation.Y);
        }

        public void SetTransformInternal(float zoom, float xTranslation, float yTranslation)
        {
            bool transformChanged = false;
            if (_zoom != zoom)
            {
                _zoom = zoom;
                transformChanged = true;
            }
            
            if (_translation.X != xTranslation || _translation.Y != xTranslation)
            {
                _translation = new PointF(xTranslation, yTranslation);
                transformChanged = true;
            }

            if (transformChanged)
            {
                UpdateMatrices();
                TransformChanged.Raise(this, EventArgs.Empty);
                if (AdaptedControl != null)
                    AdaptedControl.Invalidate();
            }
        }

        public event EventHandler TransformChanged;

        private PointF _minTranslation = new PointF(float.MinValue, float.MinValue);
        private PointF _maxTranslation = new PointF(float.MaxValue, float.MaxValue);
        private PointF _minScale = new PointF(float.MinValue, float.MinValue);
        private PointF _maxScale = new PointF(float.MaxValue, float.MaxValue);
        private bool _uniformScale = true;
        private bool _enforceConstraints = false;
        #endregion
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

            // interface wants the result in client coords, so we need to transform through
            // the canvas transformation.
            var graphControl = AdaptedControl.As<NodeEditorCore.GraphControl>();
            if (graphControl != null) {
                var pts = new PointF[] { new PointF(minX, minY), new PointF(maxX, maxY) };
                graphControl.Transform.TransformPoints(pts);
                return new Rectangle((int)pts[0].X, (int)pts[0].Y, (int)(pts[1].X - pts[0].X), (int)(pts[1].Y - pts[0].Y));
            }
            return new Rectangle((int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY));
        }
    }

    public class ViewingContext : IViewingContext, ILayoutContext
    {
        public AdaptableControl Control
        {
            get { return _control; }
            set
            {
                if (_control != null)
                {
                    _control.SizeChanged -= control_SizeChanged;
                    _control.VisibleChanged -= control_VisibleChanged;
                }

                _control = value;
                _layoutConstraints = EmptyEnumerable<ILayoutConstraint>.Instance;

                if (_control != null)
                {
                    _layoutConstraints = _control.AsAll<ILayoutConstraint>();
                    _control.SizeChanged += control_SizeChanged;
                    _control.VisibleChanged += control_VisibleChanged;
                }

                SetCanvasBounds();
            }
        }

        public Rectangle GetClientSpaceBounds(object item)
        {
            return GetClientSpaceBounds(new object[] { item });
        }

        public Rectangle GetClientSpaceBounds(IEnumerable<object> items)
        {
            Rectangle bounds = new Rectangle();
            foreach (IPickingAdapter2 pickingAdapter in _control.AsAll<IPickingAdapter2>())
            {
                Rectangle adapterBounds = pickingAdapter.GetBounds(items);
                if (!adapterBounds.IsEmpty)
                {
                    if (bounds.IsEmpty)
                        bounds = adapterBounds;
                    else
                        bounds = Rectangle.Union(bounds, adapterBounds);
                }
            }

            return bounds;
        }

        private HyperGraph.IGraphModel GetGraphModel()
        {
            var adapter = _control.As<HyperGraphAdapter>();
            if (adapter == null) return null;
            return adapter.Model;
        }

        public Rectangle GetClientSpaceBounds()
        {
            var items = new List<object>();
            var graph = GetGraphModel();
            if (graph != null)
                items.AddRange(graph.Nodes);
            return GetClientSpaceBounds(items);
        }

        public IEnumerable<object> GetVisibleItems()
        {
            Rectangle windowBounds = _control.As<ICanvasAdapter>().WindowBounds;
            foreach (IPickingAdapter2 pickingAdapter in _control.AsAll<IPickingAdapter2>())
            {
                foreach (object item in pickingAdapter.Pick(windowBounds))
                    yield return item;
            }
        }

        #region IViewingContext Members
        public bool CanFrame(IEnumerable<object> items)
        {
            return _control.As<IViewingContext>().CanFrame(items);
        }
        public void Frame(IEnumerable<object> items)
        {
            _control.As<IViewingContext>().Frame(items);
        }
        public bool CanEnsureVisible(IEnumerable<object> items)
        {
            return _control.As<IViewingContext>().CanFrame(items);
        }
        public void EnsureVisible(IEnumerable<object> items)
        {
            _control.As<IViewingContext>().EnsureVisible(items);
        }
        #endregion

        #region ILayoutContext Members

        private BoundsSpecified GetNodeBounds(HyperGraph.Node element, out Rectangle bounds)
        {
            bounds = GetClientSpaceBounds(element);

            //transform to world coordinates
            // (note -- these transforms could be avoided by calculating the bounds in canvas space directly, without using the PickingAdapter)
            var transformAdapter = _control.As<ITransformAdapter>();
            bounds = GdiUtil.InverseTransform(transformAdapter.Transform, bounds);

            return BoundsSpecified.All;
        }

        BoundsSpecified ILayoutContext.GetBounds(object item, out Rectangle bounds)
        {
            var element = item.As<HyperGraph.Node>();
            if (element != null)
                return GetNodeBounds(element, out bounds);

            bounds = new Rectangle();
            return BoundsSpecified.None;
        }

        BoundsSpecified ILayoutContext.CanSetBounds(object item)
        {
            if (item.Is<HyperGraph.Node>())
                return BoundsSpecified.Location;
            return BoundsSpecified.None;
        }

        void ILayoutContext.SetBounds(object item, Rectangle bounds, BoundsSpecified specified)
        {
            var element = item.As<HyperGraph.Node>();
            if (element != null)
            {
                Rectangle workingBounds;
                var bs = GetNodeBounds(element, out workingBounds);
                if (bs == BoundsSpecified.Location || bs == BoundsSpecified.All)
                {
                    workingBounds = WinFormsUtil.UpdateBounds(workingBounds, bounds, specified);
                    element.Location = workingBounds.Location;
                }
            }
        }
        #endregion

        private void control_VisibleChanged(object sender, EventArgs e)
        {
            SetCanvasBounds();
        }

        private void control_SizeChanged(object sender, EventArgs e)
        {
            SetCanvasBounds();
        }

        /// <summary>
        /// Updates the control CanvasAdapter's bounds</summary>
        protected virtual void SetCanvasBounds()
        {
            // update the control CanvasAdapter's bounds
            if (_control != null && _control.Visible)
            {
                Rectangle bounds = GetClientSpaceBounds();

                //transform to world coordinates
                var transformAdapter = _control.As<ITransformAdapter>();
                bounds = GdiUtil.InverseTransform(transformAdapter.Transform, bounds);

                // Make the canvas larger than it needs to be to give the user some room.
                // Use the client rectangle in world coordinates.
                Rectangle clientRect = GdiUtil.InverseTransform(transformAdapter.Transform, _control.ClientRectangle);
                bounds.Width = Math.Max(bounds.Width * 2, clientRect.Width * 2);
                bounds.Height = Math.Max(bounds.Height * 2, clientRect.Height * 2);

                var canvasAdapter = _control.As<ICanvasAdapter>();
                if (canvasAdapter != null)
                    canvasAdapter.Bounds = bounds;
            }
        }

        private AdaptableControl _control;
        private IEnumerable<ILayoutConstraint> _layoutConstraints;
    }
}
