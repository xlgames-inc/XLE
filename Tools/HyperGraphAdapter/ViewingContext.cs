// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Drawing;
using System.Collections.Generic;
using System.Windows.Forms;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Applications;

namespace HyperGraphAdapter
{
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

        private HyperGraph.IGraphModel GetViewModel()
        {
            var adapter = _control.As<HyperGraphAdapter>();
            if (adapter == null) return null;
            return adapter.Model;
        }

        public Rectangle GetClientSpaceBounds()
        {
            var items = new List<object>();
            var graph = GetViewModel();
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
