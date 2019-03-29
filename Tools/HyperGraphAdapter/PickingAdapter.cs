// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.Drawing;
using System.Collections.Generic;
using Sce.Atf.Controls.Adaptable;

namespace HyperGraphAdapter
{
    class PickingAdapter : ControlAdapter, IPickingAdapter2
    {
        private HyperGraph.IGraphModel GetViewModel()
        {
            var adapter = AdaptedControl.As<HyperGraphAdapter>();
            if (adapter == null) return null;
            return adapter.Model;
        }

        public DiagramHitRecord Pick(Point p)
        {
            var viewModel = GetViewModel();
            if (viewModel == null) return null;
            return new DiagramHitRecord(HyperGraph.GraphControl.FindElementAt(viewModel, p));
        }

        public IEnumerable<object> Pick(Rectangle bounds)
        {
            var viewModel = GetViewModel();
            if (viewModel == null) return null;
            RectangleF rectF = new RectangleF((float)bounds.X, (float)bounds.Y, (float)bounds.Width, (float)bounds.Height);
            return HyperGraph.GraphControl.RectangleSelection(viewModel, rectF);
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
            var graphControl = AdaptedControl.As<HyperGraph.GraphControl>();
            if (graphControl != null)
            {
                var pts = new PointF[] { new PointF(minX, minY), new PointF(maxX, maxY) };
                graphControl.Transform.TransformPoints(pts);
                return new Rectangle((int)pts[0].X, (int)pts[0].Y, (int)(pts[1].X - pts[0].X), (int)(pts[1].Y - pts[0].Y));
            }
            return new Rectangle((int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY));
        }
    }
}
