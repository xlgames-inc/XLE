using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Drawing;
using System.Windows.Forms;

namespace AuthoringConcept.ImmediateGUI
{
    public abstract class Frame
    {
        public void Draw(Graphics graphics, RectangleF destination)
        {
            PrepareLayout(graphics, destination.Size);
            DrawYogaHierarchy(new ImbuedGraphics { Underlying = graphics }, destination.Location);
        }

        public SizeF Measure(Graphics graphics, SizeF defaultSize)
        {
            PrepareLayout(graphics, defaultSize);
            if (!LayedOutRoots.Any())
                return SizeF.Empty;
            return new SizeF(
                LayedOutRoots.ElementAt(0).LayoutWidth,
                LayedOutRoots.ElementAt(0).LayoutHeight);
        }

        protected abstract void PerformLayout(Arbiter gui);

        private void PrepareLayout(Graphics graphics, SizeF destinationSize)
        {
            if (LayedOutRoots == null)
            {
                var gui = new Arbiter { Graphics = graphics };
                gui.Reset();

                var mainRoot = gui.BeginRoot();
                mainRoot.MinWidth = destinationSize.Width;
                mainRoot.MinHeight = destinationSize.Height;

                PerformLayout(gui);

                gui.EndRoot();

                LayedOutRoots = gui.GetRoots();
                BreadthFirstNodes.Clear();

                foreach (var n in LayedOutRoots)
                {
                    n.RootFrame?.Invoke(n);
                    n.CalculateLayout();
                }

                foreach (var n in LayedOutRoots)
                {
                    BuildBreadthFirstNodes(BreadthFirstNodes, n);
                }
            }
        }

        protected IEnumerable<ImbuedRoot> LayedOutRoots;
        protected List<ImbuedNode> BreadthFirstNodes = new List<ImbuedNode>();

        protected static void BuildBreadthFirstNodes(List<ImbuedNode> result, ImbuedNode node)
        {
            result.Add(node);
            foreach (var child in node)
            {
                BuildBreadthFirstNodes(result, child as ImbuedNode);
            }
        }

        protected void DrawYogaHierarchy(ImbuedGraphics graphics, PointF baseOffset)
        {
            foreach (var node in BreadthFirstNodes)
            {
                using (Pen myPen = new Pen(Color.Blue))
                {
                    var topLeft = Utils.GetAbsoluteTopLeft(node);
                    topLeft = new PointF(topLeft.X + baseOffset.X, topLeft.Y + baseOffset.Y);
                    if (node.Draw != null)
                    {
                        var io = new IO
                        {
                            CurrentMouseOver = CurrentMouseOver,
                            LButtonDown = ((Control.MouseButtons & MouseButtons.Left) != 0),
                            LButtonTransition = false,
                            MousePosition = Control.MousePosition
                        };
                        var frameRect = new RectangleF(topLeft.X, topLeft.Y, node.LayoutWidth, node.LayoutHeight);
                        var contentRect = new RectangleF(
                            frameRect.Left + node.LayoutPaddingLeft,
                            frameRect.Top + node.LayoutPaddingTop,
                            frameRect.Right - node.LayoutPaddingRight - (frameRect.Left + node.LayoutPaddingLeft),
                            frameRect.Bottom - node.LayoutPaddingBottom - (frameRect.Top + node.LayoutPaddingTop));
                        node.Draw(graphics, frameRect, contentRect, node.Guid, io);
                    }
                    /*else
                    {
                        graphics.Underlying.DrawRectangle(myPen, new Rectangle((int)node.LayoutX, (int)node.LayoutY, (int)node.LayoutWidth, (int)node.LayoutHeight));
                    }*/
                }
            }
        }

        public void InvalidateLayout()
        {
            LayedOutRoots = null;
        }

        protected UInt64 CurrentMouseOver = 0;

        protected void UpdateMouseOver(PointF pt)
        {
            UInt64 newMouseOver = 0;
            for (int c = 0; c < BreadthFirstNodes.Count; ++c)
            {
                var node = BreadthFirstNodes[BreadthFirstNodes.Count - 1 - c];
                if (node.Guid == 0) continue;

                var topLeft = Utils.GetAbsoluteTopLeft(node);
                if (pt.X >= topLeft.X && pt.X < (topLeft.X + node.LayoutWidth)
                    && pt.Y >= topLeft.Y && pt.Y < (topLeft.Y + node.LayoutHeight))
                {
                    newMouseOver = node.Guid;
                    break;
                }
            }

            if (CurrentMouseOver != newMouseOver)
            {
                // Consider re-layout? At the very least we need to redraw this node and the old focus node
                CurrentMouseOver = newMouseOver;
                Invalidate();       // todo -- we can invalidate just the changed nodes
            }
        }

        public event EventHandler OnInvalidate;

        protected void Invalidate()
        {
            OnInvalidate?.Invoke(this, null);
        }

        public void OnMouseMove(MouseEventArgs e)
        {
            UpdateMouseOver(e.Location);

            if (e.Button == MouseButtons.Left)
            {
                bool invalidate = false;
                foreach (var node in BreadthFirstNodes)
                {
                    if (node.Guid != CurrentMouseOver)
                        continue;

                    if (node.IO != null)
                    {
                        var io = new IO
                        {
                            CurrentMouseOver = CurrentMouseOver,
                            LButtonDown = true,
                            LButtonTransition = false,
                            MousePosition = e.Location
                        };
                        var topLeft = Utils.GetAbsoluteTopLeft(node);
                        var frameRect = new RectangleF(topLeft.X, topLeft.Y, node.LayoutWidth, node.LayoutHeight);
                        var contentRect = new RectangleF(
                            frameRect.Left + node.LayoutPaddingLeft,
                            frameRect.Top + node.LayoutPaddingTop,
                            frameRect.Right - node.LayoutPaddingRight - (frameRect.Left + node.LayoutPaddingLeft),
                            frameRect.Bottom - node.LayoutPaddingBottom - (frameRect.Top + node.LayoutPaddingTop));
                        node.IO(frameRect, contentRect, node.Guid, io);
                        invalidate = true;
                    }

                    break;  // break once we find the "CurrentMouseOver" node
                }

                if (invalidate)
                {
                    Invalidate();
                }
            }
        }

        public void OnMouseDown(MouseEventArgs e)
        {
            UpdateMouseOver(e.Location);

            if (e.Button == MouseButtons.Left)
            {
                bool redoLayout = false;
                foreach (var node in BreadthFirstNodes)
                {
                    if (node.Guid != CurrentMouseOver)
                        continue;

                    if (node.IO != null)
                    {
                        var io = new IO
                        {
                            CurrentMouseOver = CurrentMouseOver,
                            LButtonDown = true,
                            LButtonTransition = true,
                            MousePosition = e.Location
                        };
                        var topLeft = Utils.GetAbsoluteTopLeft(node);
                        var frameRect = new RectangleF(topLeft.X, topLeft.Y, node.LayoutWidth, node.LayoutHeight);
                        var contentRect = new RectangleF(
                            frameRect.Left + node.LayoutPaddingLeft,
                            frameRect.Top + node.LayoutPaddingTop,
                            frameRect.Right - node.LayoutPaddingRight - (frameRect.Left + node.LayoutPaddingLeft),
                            frameRect.Bottom - node.LayoutPaddingBottom - (frameRect.Top + node.LayoutPaddingTop));
                        node.IO(frameRect, contentRect, node.Guid, io);
                        redoLayout = true;
                    }

                    break;  // break once we find the "CurrentMouseOver" node
                }

                if (redoLayout)
                {
                    LayedOutRoots = null;
                    Invalidate();
                }
            }
            else if (e.Button == MouseButtons.Right)
            {
                LayedOutRoots = null;
                Invalidate();
            }
        }

        public void OnMouseUp(MouseEventArgs e)
        {
            UpdateMouseOver(e.Location);

            if (e.Button == MouseButtons.Left)
            {
                bool redoLayout = false;
                foreach (var node in BreadthFirstNodes)
                {
                    if (node.Guid != CurrentMouseOver)
                        continue;

                    if (node.IO != null)
                    {
                        var io = new IO
                        {
                            CurrentMouseOver = CurrentMouseOver,
                            LButtonDown = false,
                            LButtonTransition = true,
                            MousePosition = e.Location
                        };
                        var topLeft = Utils.GetAbsoluteTopLeft(node);
                        var frameRect = new RectangleF(topLeft.X, topLeft.Y, node.LayoutWidth, node.LayoutHeight);
                        var contentRect = new RectangleF(
                            frameRect.Left + node.LayoutPaddingLeft,
                            frameRect.Top + node.LayoutPaddingTop,
                            frameRect.Right - node.LayoutPaddingRight - (frameRect.Left + node.LayoutPaddingLeft),
                            frameRect.Bottom - node.LayoutPaddingBottom - (frameRect.Top + node.LayoutPaddingTop));
                        node.IO(frameRect, contentRect, node.Guid, io);
                        redoLayout = true;
                    }
                }

                if (redoLayout)
                {
                    LayedOutRoots = null;
                    Invalidate();
                }
            }
        }

        public void OnMouseDoubleClick(MouseEventArgs e, Control parentControl, System.Drawing.Drawing2D.Matrix frameToControl)
        {
            UpdateMouseOver(e.Location);

            if (e.Button == MouseButtons.Left)
            {
                bool redoLayout = false;
                foreach (var node in BreadthFirstNodes)
                {
                    if (node.Guid != CurrentMouseOver)
                        continue;

                    if (node.IO != null)
                    {
                        var io = new IO
                        {
                            CurrentMouseOver = CurrentMouseOver,
                            LButtonDown = false,
                            LButtonTransition = false,
                            LButtonDblClk = true,
                            MousePosition = e.Location,
                            ParentControl = parentControl,
                            LocalToParentControl = frameToControl
                        };
                        var topLeft = Utils.GetAbsoluteTopLeft(node);
                        var frameRect = new RectangleF(topLeft.X, topLeft.Y, node.LayoutWidth, node.LayoutHeight);
                        var contentRect = new RectangleF(
                            frameRect.Left + node.LayoutPaddingLeft,
                            frameRect.Top + node.LayoutPaddingTop,
                            frameRect.Right - node.LayoutPaddingRight - (frameRect.Left + node.LayoutPaddingLeft),
                            frameRect.Bottom - node.LayoutPaddingBottom - (frameRect.Top + node.LayoutPaddingTop));
                        node.IO(frameRect, contentRect, node.Guid, io);
                        redoLayout = true;
                    }

                    break;  // break once we find the "CurrentMouseOver" node
                }

                if (redoLayout)
                {
                    LayedOutRoots = null;
                    Invalidate();
                }
            }
            else if (e.Button == MouseButtons.Right)
            {
                LayedOutRoots = null;
                Invalidate();
            }
        }
    }
}
