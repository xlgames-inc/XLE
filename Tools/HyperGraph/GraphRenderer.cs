﻿#region License
// Copyright (c) 2009 Sander van Rossen, 2013 Oliver Salzburg
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#endregion

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace HyperGraph
{
	public static class GraphRenderer
	{
        private enum NodeColumns { Input, Center, Output };
		static IEnumerable<NodeItem> EnumerateNodeItems(Node node, NodeColumns column)
		{
			if (node == null)
				yield break;

            if (node.Collapsed)
            {
                if (column == NodeColumns.Center && node.TitleItem != null)
                    yield return node.TitleItem;
                yield break;
            }

            if (column == NodeColumns.Input)
            {
                foreach (var item in node.ItemsForDock(Node.Dock.Input))
                    yield return item;
            }
            else if (column == NodeColumns.Output)
            {
                foreach (var item in node.ItemsForDock(Node.Dock.Output))
                    yield return item;
            } else
            {
                foreach (var item in node.ItemsForDock(Node.Dock.Top))
                    yield return item;
                foreach (var item in node.ItemsForDock(Node.Dock.Center))
                    yield return item;
                if ((node.state & RenderState.Hover) != 0)
                    foreach (var item in node.ItemsForDock(Node.Dock.Bottom))
                        yield return item;
            }
        }

        private struct NodeSize
        {
            internal SizeF BaseSize { get; set; }
            internal float InputPartWidth { get; set; }
            internal float OutputPartWidth { get; set; }
        };

        private static NodeColumns NodeColumnForIndex(uint index)
        {
            return (NodeColumns)Enum.GetValues(typeof(NodeColumns)).GetValue(index);
        }

        private static NodeSize MeasureRectangular(Graphics context, Node node)
        {
            if (node == null)
                return new NodeSize { BaseSize = SizeF.Empty, InputPartWidth = 0, OutputPartWidth = 0 };

            SizeF size = Size.Empty;
            size.Height = node.Collapsed ? GraphConstants.TopHeightCollapsed : GraphConstants.TopHeight;

            bool[] firstItem = new bool[3] { true, true, true };
            SizeF[] sizes = new SizeF[3] { size, size, size };
            for (uint side = 0; side<3; ++side)
            {
                foreach (var item in EnumerateNodeItems(node, NodeColumnForIndex(side)))
                {
                    var itemSize = item.Measure(context);

                    if (side != 1)
                        itemSize = AdjustConnectorSize(itemSize);

                    sizes[side].Width = Math.Max(sizes[side].Width, itemSize.Width);
                    if (!firstItem[side])
                        sizes[side].Height += GraphConstants.ItemSpacing;
                    sizes[side].Height += itemSize.Height;
                    firstItem[side] = false;
                }
            }

            if ((node.state & RenderState.Hover) == 0)
                foreach (var i in node.BottomItems)
                    i.bounds = RectangleF.Empty;

            size = sizes[1];
            for (uint c = 0; c < 3; ++c)
                size.Height = Math.Max(size.Height, sizes[c].Height);

            size.Width += GraphConstants.NodeExtraWidth;
            size.Height += node.Collapsed ? GraphConstants.BottomHeightCollapsed : GraphConstants.BottomHeight;

            return new NodeSize { BaseSize = size, InputPartWidth = sizes[0].Width, OutputPartWidth = sizes[2].Width };
        }

        private static NodeSize MeasureCircular(Graphics context, Node node)
        {
            return new NodeSize { BaseSize = new SizeF ( 192, 192), InputPartWidth = 0, OutputPartWidth = 0 };
        }

        static void RenderItem(Graphics graphics, NodeItem item, object context)
		{
            if (item.bounds != RectangleF.Empty)
            {
                var connector = item as NodeConnector;
                if (connector != null)
                {
                    RenderConnector(graphics, connector.bounds, connector.state, IsInputConnector(connector) ? ConnectorType.Input : ConnectorType.Output, connector, connector.Node.Collapsed);
                }
                else
                {
                    item.Render(graphics, item.bounds, context);
                }
            }
		}

		internal static Pen BorderPen = new Pen(Color.FromArgb(96, 96, 96));
        internal static Pen ConnectionBorderPen = new Pen(Color.FromArgb(96, 96, 96)) { Width = 0.5f };

        internal static Brush DraggingBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(140, 120, 120),  Color.FromArgb(96, 96, 96));
        internal static Brush HoverBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(80, 80, 80), Color.FromArgb(96, 96, 96));
        internal static Brush NormalBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(120, 120, 120),  Color.FromArgb(96, 96, 96));
        internal static Brush TitleAreaBrush = new SolidBrush(Color.FromArgb(96, 96, 96));
        internal static Brush NullAreaBrush = new HatchBrush(HatchStyle.ForwardDiagonal, Color.FromArgb(128, 192, 192, 192), Color.FromArgb(0, 96, 96, 96));

        internal static Pen FocusPen = new Pen(Color.FromArgb(255, 255, 255), 3.0f);
        internal static Pen DottedPen = new Pen(Color.FromArgb(200, 200, 200)) { DashStyle = DashStyle.Dash, Width = 4 };
        internal static Pen ThinDottedPen = new Pen(Color.FromArgb(128, 128, 128)) { DashStyle = DashStyle.Dash, Width = 1 };

        internal static Pen SubGraphOutline = new Pen(Color.FromArgb(164, 164, 164)) { Width = 6 };

        enum ConnectorType { Input, Output };

        static GraphicsPath MakePathForConnector(RectangleF bounds, RenderState state, ConnectorType type, out Rectangle statusRect, out RectangleF clientRect)
        {
            var path = new GraphicsPath(FillMode.Winding);
            var cornerSize = GraphConstants.ConnectorCornerSize;

            if (type == ConnectorType.Input)
            {
                path.AddLine(bounds.Right, bounds.Top, bounds.Left + cornerSize, bounds.Top);
                path.AddArc(bounds.Left, bounds.Top, cornerSize, cornerSize, 270, -90);
                path.AddLine(bounds.Left, bounds.Top + cornerSize, bounds.Left, bounds.Bottom - cornerSize);
                path.AddArc(bounds.Left, bounds.Bottom - cornerSize, cornerSize, cornerSize, 180, -90);
                path.AddLine(bounds.Left + cornerSize, bounds.Bottom, bounds.Right, bounds.Bottom);

                int width = (int)(bounds.Bottom - bounds.Top - 8);
                statusRect = new Rectangle((int)(bounds.Left + 4), (int)(bounds.Top + 4), width, (int)(bounds.Height - 8));
                clientRect = new RectangleF(bounds.Left + 8 + width, bounds.Top + 4, bounds.Width - 12 - width, bounds.Height - 8);
            }
            else
            {
                path.AddLine(bounds.Left, bounds.Top, bounds.Right - cornerSize, bounds.Top);
                path.AddArc(bounds.Right - cornerSize, bounds.Top, cornerSize, cornerSize, 270, 90);
                path.AddLine(bounds.Right, bounds.Top + cornerSize, bounds.Right, bounds.Bottom - cornerSize);
                path.AddArc(bounds.Right - cornerSize, bounds.Bottom - cornerSize, cornerSize, cornerSize, 0, 90);
                path.AddLine(bounds.Right - cornerSize, bounds.Bottom, bounds.Left, bounds.Bottom);

                int width = (int)(bounds.Bottom - bounds.Top - 8);
                statusRect = new Rectangle((int)(bounds.Right - 4 - width), (int)(bounds.Top + 4), width, (int)(bounds.Height - 8));
                clientRect = new RectangleF(bounds.Left + 4, bounds.Top + 4, bounds.Width - 12 - width, bounds.Height - 8);
            }

            return path;
        }

        static void RenderConnector(Graphics graphics, RectangleF bounds, RenderState state, ConnectorType type, NodeConnector connector=null, bool collapsed=false)
		{
            if (collapsed)
            {
                using (var brush = new SolidBrush(GetArrowLineColor(state)))
                    graphics.FillEllipse(brush, bounds);
                return;
            }

			using (var brush = new SolidBrush(GetArrowLineColor(state)))
			{
                Rectangle statusRect;
                RectangleF clientRect;
                using (var path = MakePathForConnector(bounds, state, type, out statusRect, out clientRect))
                {
                    graphics.FillPath(((state & RenderState.Hover)!=0) ? HoverBrush : NormalBrush , path);
                    graphics.DrawPath(ConnectionBorderPen, path);
                    graphics.FillEllipse(brush, statusRect);

                    if (connector!=null)
                    {
                        connector.Render(graphics, clientRect, null);
                    }
                }
			}
		}

        private static SizeF AdjustConnectorSize(SizeF clientSize)
        {
            // Given the requested client size, what is the full size of the
            // connector required?
            SizeF result = clientSize;
            // result.Height += 8;
            result.Width += 12 + (result.Height - 8);   // add space for the statusRect
            return result;
        }

        public static PointF ConnectorInterfacePoint(NodeConnector connector, bool isInput)
        {
            if (connector == null) return new PointF(0.0f, 0.0f);

            RectangleF bounds;
            if (connector.Node.Collapsed)
            {
                return new PointF(
                    (connector.bounds.Left + connector.bounds.Right) * 0.5f,
                    (connector.bounds.Top + connector.bounds.Bottom) * 0.5f);
            } else
                bounds = connector.bounds;

            float width = bounds.Bottom - bounds.Top - 8;
            if (isInput) {
                return new PointF(bounds.Left + width / 2.0f + 4.0f, (bounds.Top + bounds.Bottom) / 2.0f);
            } else {
                return new PointF(bounds.Right - width / 2.0f + 4.0f, (bounds.Top + bounds.Bottom) / 2.0f);
            }
        }

		static void RenderArrow(Graphics graphics, RectangleF bounds, RenderState connectionState)
		{
			var x = (bounds.Left + bounds.Right) / 2.0f;
			var y = (bounds.Top + bounds.Bottom) / 2.0f;
			using (var brush = new SolidBrush(GetArrowLineColor(connectionState | RenderState.Connected)))
			{
				graphics.FillPolygon(brush, GetArrowPoints(x,y), FillMode.Winding);
			}
		}

		public static void PerformLayout(Graphics graphics, IGraphModel model)
		{
            foreach (var node in model.Nodes.Reverse<Node>())
			{
				GraphRenderer.PerformLayout(graphics, node);
			}

            foreach (var subGraph in model.SubGraphs.Reverse<Node>())
            {
                GraphRenderer.PerformSubGraphLayout(graphics, subGraph, model);
            }
        }

		public static void Render(Graphics graphics, IGraphModel model, bool showLabels, object context)
		{
			var skipConnections = new HashSet<NodeConnection>();

            using (Region myRegion = graphics.Clip.Clone())
            {
                foreach (var node in model.SubGraphs)
                {
                    var cornerSize = GraphConstants.SubGraphCornerSize;
                    var left = node.bounds.Location.X;
                    var top = node.bounds.Location.Y;
                    var right = node.bounds.Location.X + node.bounds.Size.Width;
                    var bottom = node.bounds.Location.Y + node.bounds.Size.Height;

                    using (var path = new GraphicsPath(FillMode.Winding))
                    {
                        path.AddArc(left, top, cornerSize, cornerSize, 180, 90);
                        path.AddArc(right - cornerSize, top, cornerSize, cornerSize, 270, 90);

                        path.AddArc(right - cornerSize, bottom - cornerSize, cornerSize, cornerSize, 0, 90);
                        path.AddArc(left, bottom - cornerSize, cornerSize, cornerSize, 90, 90);
                        path.CloseFigure();

                        myRegion.Exclude(path);
                    }
                }

                graphics.FillRegion(NullAreaBrush, myRegion);
            }

            foreach (var node in model.SubGraphs.Reverse<Node>())
            {
                GraphRenderer.RenderSubGraph(graphics, node, context);
            }
            foreach (var node in model.SubGraphs.Reverse<Node>())
            {
                GraphRenderer.RenderConnections(graphics, node, skipConnections, showLabels);
            }

            foreach (var node in model.Nodes.Reverse<Node>())
			{
				GraphRenderer.RenderConnections(graphics, node, skipConnections, showLabels);
			}
			foreach (var node in model.Nodes.Reverse<Node>())
			{
				GraphRenderer.Render(graphics, node, context);
			}
		}

        private static RectangleF CollapsedInputConnector(RectangleF nodeBounds, uint index, uint count)
        {
            var center = new PointF(nodeBounds.Left, (nodeBounds.Top + nodeBounds.Bottom) / 2.0f);
            float arcRadiusY = (nodeBounds.Bottom - nodeBounds.Top) / 2.0f;
            float arcRadiusX = 0.5f * arcRadiusY;
            double angle = (index+.5f) / (double)count * Math.PI;
            center = new PointF(center.X - arcRadiusX * (float)Math.Sin(angle), center.Y - arcRadiusY * (float)Math.Cos(angle));
            float radius = 4.0f;
            return new RectangleF(center.X - radius, center.Y - radius, 2.0f * radius, 2.0f * radius);
        }

        private static RectangleF CollapsedOutputConnector(RectangleF nodeBounds, uint index, uint count)
        {
            var center = new PointF(nodeBounds.Right, (nodeBounds.Top + nodeBounds.Bottom) / 2.0f);
            float arcRadiusY = (nodeBounds.Bottom - nodeBounds.Top) / 2.0f;
            float arcRadiusX = 0.5f * arcRadiusY;
            double angle = (index+0.5f) / (double)count * Math.PI;
            center = new PointF(center.X + arcRadiusX * (float)Math.Sin(angle), center.Y - arcRadiusY * (float)Math.Cos(angle));
            float radius = 4.0f;
            return new RectangleF(center.X - radius, center.Y - radius, 2.0f * radius, 2.0f * radius);
        }

        public static void PerformLayout(Graphics graphics, Node node)
        {
            if (node.Layout == Node.LayoutType.Rectangular || node.Collapsed)
            {
                var size = MeasureRectangular(graphics, node);
                var position = node.Location;
                node.bounds = new RectangleF(position, size.BaseSize);
                position.Y += node.Collapsed ? GraphConstants.TopHeightCollapsed : GraphConstants.TopHeight;

                if (node.Collapsed)
                {
                    uint inputConnectorCount = (uint)node.InputConnectors.Count(), outputConnectorCount = (uint)node.OutputConnectors.Count();

                    uint inputConnectorIndex = 0, outputConnectorIndex = 0;
                    foreach (var item in node.InputConnectors)
                    {
                        item.bounds = CollapsedInputConnector(node.bounds, inputConnectorIndex, inputConnectorCount);
                        ++inputConnectorIndex;
                    }

                    foreach (var item in node.OutputConnectors)
                    {
                        item.bounds = CollapsedOutputConnector(node.bounds, outputConnectorIndex, outputConnectorCount);
                        ++outputConnectorIndex;
                    }

                    if (node.TitleItem != null)
                    {
                        node.TitleItem.bounds = new RectangleF(position, node.TitleItem.Measure(graphics));
                        node.TitleItem.bounds.Width = System.Math.Max(node.TitleItem.bounds.Width, node.bounds.Width);
                    }
                }
                else
                {
                    PointF[] positions = new PointF[] { new PointF(position.X - size.InputPartWidth, position.Y), new PointF(position.X + GraphConstants.HorizontalSpacing, position.Y), new PointF(position.X + size.BaseSize.Width, position.Y) };
                    float[] widths = new float[] { size.InputPartWidth, size.BaseSize.Width - 4, size.OutputPartWidth };

                    for (uint side = 0; side < 3; ++side)
                    {
                        bool first = true;
                        foreach (var item in EnumerateNodeItems(node, NodeColumnForIndex(side)))
                        {
                            if (!first) positions[side].Y += GraphConstants.ItemSpacing;
                            first = false;
                            var itemSize = item.Measure(graphics);
                            itemSize.Width = System.Math.Max(itemSize.Width, widths[side]);
                            item.bounds = new RectangleF(positions[side], itemSize);
                            positions[side].Y += itemSize.Height;
                        }
                    }
                }
            }
            else
            {
                PerformCircularLayout(graphics, node);
            }
        }

        private enum VerticalItemLayoutSide { LeftOfAnchor, RightOfAnchor };
        private static float LayoutItemsVertically(Graphics graphics, IEnumerable<NodeItem> items, PointF anchor, bool centerAroundAnchor, VerticalItemLayoutSide layoutType)
        {
            var inputRects = items.Select(x => AdjustConnectorSize(x.Measure(graphics)));

            float groupHeight = 0.0f, groupWidth = 0.0f;
            bool firstItem = true;
            foreach (var i in inputRects)
            {
                if (!firstItem) groupHeight += GraphConstants.ItemSpacing;
                firstItem = false;
                groupHeight += i.Height;
                groupWidth = Math.Max(i.Width, groupWidth);
            }

            float y = anchor.Y;
            if (centerAroundAnchor)
                y -= groupHeight / 2.0f;
            using (var list1enum = items.GetEnumerator())
            using (var list2enum = inputRects.GetEnumerator())
                while (list1enum.MoveNext() && list2enum.MoveNext())
                {
                    var rect = list2enum.Current;
                    if (layoutType == VerticalItemLayoutSide.LeftOfAnchor)
                        list1enum.Current.bounds = new RectangleF(anchor.X - groupWidth, y, groupWidth, rect.Height);
                    else
                        list1enum.Current.bounds = new RectangleF(anchor.X, y, groupWidth, rect.Height);
                    y += rect.Height + GraphConstants.ItemSpacing;
                }

            return groupWidth;
        }

        private enum HorizontalItemLayoutSide { AboveAnchor, BelowAnchor };
        private static float LayoutItemsHorizontally(Graphics graphics, IEnumerable<NodeItem> items, float left, float right, float verticalAnchor, HorizontalItemLayoutSide layoutSide)
        {
            var inputRects = items.Select(x => x.Measure(graphics));

            float groupHeight = 0.0f, groupWidth = 0.0f;
            bool firstItem = true;
            foreach (var i in inputRects)
            {
                if (!firstItem) groupWidth += GraphConstants.ItemSpacing;
                firstItem = false;
                groupWidth += i.Width;
                groupHeight = Math.Max(i.Height, groupHeight);
            }

            float y = verticalAnchor;
            if (layoutSide == HorizontalItemLayoutSide.AboveAnchor) y -= groupHeight;

            float xIterator = (left + right - groupWidth) / 2.0f;
            using (var list1enum = items.GetEnumerator())
            using (var list2enum = inputRects.GetEnumerator())
                while (list1enum.MoveNext() && list2enum.MoveNext())
                {
                    var rect = list2enum.Current;
                    list1enum.Current.bounds = new RectangleF(xIterator, y, rect.Width, groupHeight);
                    xIterator += rect.Width + GraphConstants.ItemSpacing;
                }

            return groupHeight;
        }

        private static void PerformCircularLayout(Graphics graphics, Node node)
        {
            node.bounds = new RectangleF(node.Location, MeasureCircular(graphics, node).BaseSize);

            LayoutItemsVertically(
                graphics, node.InputItems,
                new PointF(node.bounds.Left, node.bounds.Top + node.bounds.Height / 2.0f),
                true, VerticalItemLayoutSide.LeftOfAnchor);

            LayoutItemsVertically(
                graphics, node.OutputItems,
                new PointF(node.bounds.Right, node.bounds.Top + node.bounds.Height / 2.0f),
                true, VerticalItemLayoutSide.RightOfAnchor);

            double inflate = (1.4142135623731d - 1.0d) / 2.0d * Math.Max(node.bounds.Size.Width, node.bounds.Size.Height);
            var centerItemBoundary = node.bounds;
            centerItemBoundary.Inflate((float)inflate, (float)inflate);
            foreach (var i in node.CenterItems)
                i.bounds = centerItemBoundary;

            LayoutItemsHorizontally(
                graphics, node.TopItems,
                node.bounds.Left, node.bounds.Right, node.bounds.Top,
                HorizontalItemLayoutSide.BelowAnchor);

            LayoutItemsHorizontally(
                graphics, node.BottomItems,
                node.bounds.Left, node.bounds.Right, node.bounds.Bottom,
                HorizontalItemLayoutSide.AboveAnchor);
        }

        static void RenderOutline(Graphics graphics, Node node, object context)
        {
            using (var path = new GraphicsPath(FillMode.Winding))
            {
                Rectangle statusRect; RectangleF clientRect;
                foreach (var item in node.ItemsForDock(Node.Dock.Input))    // (don't use EnumerateItems because we want to show collapsed nodes)
                {
                    // note --  the "connected" state is not stored in the retained state member
                    //          ... so if we want to colour the connectors based on if they are connected,
                    //          we need to look for connections now.
                    var inputConnector = item as NodeConnector;
                    if (inputConnector != null && !inputConnector.bounds.IsEmpty)
                        graphics.DrawPath(DottedPen, MakePathForConnector(inputConnector.bounds, inputConnector.state, ConnectorType.Input, out statusRect, out clientRect));
                }

                foreach (var item in node.ItemsForDock(Node.Dock.Output))
                {
                    var outputConnector = item as NodeConnector;
                    if (outputConnector != null && !outputConnector.bounds.IsEmpty)
                        graphics.DrawPath(DottedPen, MakePathForConnector(outputConnector.bounds, outputConnector.state, ConnectorType.Output, out statusRect, out clientRect));
                }

                graphics.DrawRectangle(DottedPen, Rectangle.Round(node.bounds));
            }
        }


        static void Render(Graphics graphics, Node node, object context)
		{
            Brush brush;
            if ((node.state & RenderState.Dragging) != 0)       { brush = DraggingBrush; }
            // else if ((node.state & RenderState.Hover) != 0)     { brush = HoverBrush; }
            else                                                { brush = NormalBrush; }

            if (node.SubGraphTag == null)
            {
                RenderOutline(graphics, node, context);
                if (node.TitleItem != null)
                    RenderItem(graphics, node.TitleItem, context);
                return;
            }

            var rect = node.bounds;
            if (node.Collapsed)
            {
                graphics.FillRectangle(TitleAreaBrush, rect);
            }
            else
            {
                int titleHeight = (node.TitleItem != null) ? (int)(node.TitleItem.bounds.Height + GraphConstants.TopHeight) : 0;
                titleHeight = Math.Min((int)rect.Height, titleHeight);

                var titleRect = new Rectangle((int)rect.Left, (int)rect.Top, (int)rect.Width, titleHeight);
                var backgroundRect = new Rectangle((int)rect.Left, (int)rect.Top + titleHeight, (int)rect.Width, (int)rect.Height - titleHeight);

                DrawShadow(graphics, Rectangle.Round(rect));
                graphics.FillRectangle(brush, backgroundRect);
                if (titleHeight != 0)
                    graphics.FillRectangle(TitleAreaBrush, titleRect);
                graphics.DrawRectangle(BorderPen, Rectangle.Round(rect));
            }

            var isFocused = (node.state & RenderState.Focus) != 0;
            if (!node.Collapsed || isFocused)
            {
                var outlinePen = isFocused ? FocusPen : ThinDottedPen;

                // We're going to draw a outline around the edge of the entire node
                // This should be a thick line, with some bright color
                if (node.Layout == Node.LayoutType.Rectangular || node.Collapsed)
                {
                    var outline = rect;
                    outline.Inflate(4, 4);
                    graphics.DrawRectangle(outlinePen, Rectangle.Round(outline));
                }
                else
                {
                    var outline = rect;
                    double inflate = (1.4142135623731d - 1.0d) / 2.0d * Math.Max(outline.Size.Width, outline.Size.Height);
                    outline.Inflate((float)inflate + 4.0f, (float)inflate + 4.0f);
                    graphics.DrawEllipse(outlinePen, outline);
                }
            }

            if (node.Collapsed)
            {
                if (node.TitleItem != null)
                    RenderItem(graphics, node.TitleItem, context);

                foreach (var item in node.InputItems)
                {
                    var connector = item as NodeConnector;
                    if (connector != null)
                        RenderConnector(graphics, connector.bounds, connector.state, ConnectorType.Input, connector, true);
                }

                foreach (var item in node.OutputItems)
                {
                    var connector = item as NodeConnector;
                    if (connector != null)
                        RenderConnector(graphics, connector.bounds, connector.state, ConnectorType.Output, connector, true);
                }
            }
            else
            {
                foreach (var item in node.CenterItems.Concat(node.TopItems).Concat(node.BottomItems))
                    RenderItem(graphics, item, context);

                foreach (var item in node.InputItems)    // (don't use EnumerateItems because we want to show collapsed nodes)
                    RenderItem(graphics, item, context);

                foreach (var item in node.OutputItems)
                    RenderItem(graphics, item, context);
            }
		}

        static void RenderSubGraph(Graphics graphics, Node node, object context)
        {
            var size = node.bounds.Size;
            var position = node.bounds.Location;

            var cornerSize = GraphConstants.SubGraphCornerSize;
            var left = position.X;
            var top = position.Y;
            var right = position.X + size.Width;
            var bottom = position.Y + size.Height;

            using (var path = new GraphicsPath(FillMode.Winding))
            {
                path.AddArc(left, top, cornerSize, cornerSize, 180, 90);
                path.AddArc(right - cornerSize, top, cornerSize, cornerSize, 270, 90);

                path.AddArc(right - cornerSize, bottom - cornerSize, cornerSize, cornerSize, 0, 90);
                path.AddArc(left, bottom - cornerSize, cornerSize, cornerSize, 90, 90);
                path.CloseFigure();

                graphics.DrawPath(SubGraphOutline, path);
            }

            foreach (var item in node.ItemsForDock(Node.Dock.Input))
                RenderItem(graphics, item, context);

            foreach (var item in node.ItemsForDock(Node.Dock.Output))
                RenderItem(graphics, item, context);

            foreach (var item in node.ItemsForDock(Node.Dock.Top).Concat(node.ItemsForDock(Node.Dock.Bottom)))
                RenderItem(graphics, item, context);
        }

        public static void PerformSubGraphLayout(Graphics graphics, Node node, IGraphModel model)
        {
            if (node == null)
                return;

            float minX = System.Single.MaxValue, minY = System.Single.MaxValue;
            float maxX = System.Single.MinValue, maxY = System.Single.MinValue;

            foreach (var n in model.Nodes)
            {
                if (n.SubGraphTag == node.SubGraphTag)
                {
                    minX = Math.Min(n.bounds.Left, minX);
                    minY = Math.Min(n.bounds.Top, minY);
                    maxX = Math.Max(n.bounds.Right, maxX);
                    maxY = Math.Max(n.bounds.Bottom, maxY);
                }
            }

            float[] widths = new float[] { 8, 8 };
            NodeColumns[] nodeColumns = new NodeColumns[] { NodeColumns.Input, NodeColumns.Output };
            for (uint side = 0; side < 2; ++side)
            {
                foreach (var item in EnumerateNodeItems(node, nodeColumns[side]))
                {
                    var itemSize = AdjustConnectorSize(item.Measure(graphics));
                    widths[side] = Math.Max(widths[side], itemSize.Width);
                }
            }

            float leftBorder = widths[1] + 256, rightBorder = widths[0] + 256;

            if (minX < maxX && minY < maxY)
            {
                maxX = Math.Max(minX + 128, maxX);
                maxY = Math.Max(minY + 32, maxY);
                minY -= 96;
                maxY += 96;

                minY -= 8 + LayoutItemsHorizontally(
                    graphics, node.TopItems,
                    minX, maxX, minY,
                    HorizontalItemLayoutSide.AboveAnchor);
            }
            else
            {
                // We didn't find any valid internal nodes. Fallback to
                // default size, and maintain the same current top-left position
                minX = node.Location.X + leftBorder;
                minY = node.Location.Y;

                maxX = Math.Max(minX + 128, maxX);
                maxY = Math.Max(minY + 256, maxY);

                LayoutItemsHorizontally(
                    graphics, node.TopItems,
                    minX, maxX, minY + 8,
                    HorizontalItemLayoutSide.BelowAnchor);
            }

            node.Location = new PointF(minX - leftBorder, minY);
            node.bounds = new RectangleF(
                minX - leftBorder, minY,
                (maxX + rightBorder) - (minX - leftBorder),
                maxY - minY);

            var path = new GraphicsPath(FillMode.Winding);

            PointF[] positions = new PointF[] { new PointF(maxX + rightBorder - widths[0], minY + 32), new PointF(node.Location.X, minY + 32) };
            for (uint side = 0; side < 2; ++side)
            {
                foreach (var item in EnumerateNodeItems(node, nodeColumns[side]))
                {
                    var itemSize = item.Measure(graphics);
                    item.bounds = new RectangleF(positions[side], itemSize);
                    item.bounds.Width = System.Math.Max(item.bounds.Width, widths[side]);
                    positions[side].Y += itemSize.Height + GraphConstants.ItemSpacing;
                }
            }
        }

        public static void RenderConnections(Graphics graphics, Node node, HashSet<NodeConnection> skipConnections, bool showLabels)
		{
			foreach (var connection in node.Connections.Reverse<NodeConnection>())
			{
				if (connection == null)
					continue;

				if (skipConnections.Add(connection))
				{
					var to		= connection.To;
					var from	= connection.From;

                    var pt1 = ConnectorInterfacePoint(from, false);
                    var pt2 = ConnectorInterfacePoint(to, true);

                    if (to != null && from != null)
                    {
                        float centerX;
                        float centerY;
                        using (var path = GetArrowLinePath(pt1.X, pt1.Y, pt2.X, pt2.Y, out centerX, out centerY, false))
                        {
                            using (var brush = new SolidBrush(GetArrowLineColor(connection.state | RenderState.Connected)))
                            {
                                graphics.FillPath(brush, path);
                                graphics.DrawPath(ConnectionBorderPen, path);
                            }
                            connection.bounds = path.GetBounds();
                        }

                        if (showLabels &&
                            !string.IsNullOrWhiteSpace(connection.Text))
                        {
                            var center = new PointF(centerX, centerY);
                            RenderLabel(graphics, connection, connection.Text, center, connection.state);
                        }
                    }
                    else if (to!=null || from!=null)
                    {
                        //  this is a basic connection. It just connects one connector to a 
                        //  static string value (either a variable name or constant value)
                        //      Draw a small arrow from the connection label to the connector
                        if (!string.IsNullOrWhiteSpace(connection.Text) && !to.Node.Collapsed)
                        {
                            PointF center;
                            if (to != null) center = pt2;
                            else if (from != null) center = pt1;
                            else center = new PointF();
                            center.X -= 16;
                            RenderLabel(graphics, connection, "= " + connection.Text, center, connection.state);
                        }
                    }
				}
			}
		}

		static void RenderLabel(Graphics graphics, NodeConnection connection, string text, PointF center, RenderState state)
		{
			using (var path = new GraphicsPath(FillMode.Winding))
			{			
				int cornerSize			= (int)GraphConstants.CornerSize * 2;

                var stringFormat = GraphConstants.RightTextStringFormat; // CenterTextStringFormat;
                var size = graphics.MeasureString(text, SystemFonts.StatusFont, center, stringFormat);
                RectangleF position = connection.textBounds = new RectangleF(new PointF(center.X - size.Width, center.Y - size.Height/2), size);

                path.AddArc(position.Left,                  position.Top,   cornerSize, cornerSize, 180, 90);
                path.AddArc(position.Right - cornerSize,    position.Top,   cornerSize, cornerSize, 270, 90);

                path.AddArc(position.Right - cornerSize,                    position.Bottom - cornerSize,   cornerSize, cornerSize, 0, 90);
                path.AddArc(position.Left, position.Bottom - cornerSize,    cornerSize,                     cornerSize, 90, 90);
				path.CloseFigure();

				using (var brush = new SolidBrush(GetArrowLineColor(state)))
				{
					graphics.FillPath(brush, path);
				}
                graphics.DrawString(text, SystemFonts.StatusFont, Brushes.Black, position, GraphConstants.CenterTextStringFormat);

				if (state == RenderState.None)
					graphics.DrawPath(Pens.Black, path);

				//graphics.DrawRectangle(Pens.Black, connection.textBounds.Left, connection.textBounds.Top, connection.textBounds.Width, connection.textBounds.Height);
			}
		}

		public static Region GetConnectionRegion(NodeConnection connection)
		{
            var pt1 = ConnectorInterfacePoint(connection.From, false);
            var pt2 = ConnectorInterfacePoint(connection.To, true);

            Region region;
			float centerX;
			float centerY;
            using (var linePath = GetArrowLinePath(pt1.X, pt1.Y, pt2.X, pt2.Y, out centerX, out centerY, true, 5.0f))
			{
				region = new Region(linePath);
			}
			return region;
		}

		static Color GetArrowLineColor(RenderState state)
		{
            if ((state & (RenderState.Hover | RenderState.Dragging)) != 0)
			{
				if ((state & RenderState.Incompatible) != 0)
				{
					return Color.Red;
				} else
                if ((state & RenderState.Conversion) != 0)
                {
                    return Color.LemonChiffon;
                } else 
                if ((state & RenderState.Compatible) != 0)
				{
					return Color.LemonChiffon;
				} else
                    return Color.LemonChiffon;
			} else
			if ((state & RenderState.Incompatible) != 0)
			{
				return Color.Gray;
			} else
			if ((state & RenderState.Compatible) != 0)
			{
                return Color.ForestGreen;
			} else
            if ((state & RenderState.Conversion) != 0)
            {
                return Color.LightSkyBlue;
            } else
				return Color.LightGray;
		}
		
		static PointF[] GetArrowPoints(float x, float y, float extra_thickness = 0)
		{
			return new PointF[]{
					new PointF(x - extra_thickness, y + extra_thickness),
					new PointF(x + 1.0f + extra_thickness, y),
					new PointF(x - extra_thickness, y - extra_thickness)};
		}

		static List<PointF> GetArrowLinePoints(float x1, float y1, float x2, float y2, out float centerX, out float centerY, float extra_thickness = 0)
        {
            var widthX = (x2 - x1);
            var lengthX = Math.Max(30, Math.Abs(widthX / 2))
                //+ Math.Max(0, -widthX / 2)
                ;
            var lengthY = 0;// Math.Max(-170, Math.Min(-120.0f, widthX - 120.0f)) + 120.0f; 
            // if (widthX < 120)
            //     lengthX = 60;
            var yB = ((y1 + y2) / 2) + lengthY;// (y2 + ((y1 - y2) / 2) * 0.75f) + lengthY;
            var yC = y2 + yB;
            var xC = (x1 + x2) / 2;
            var xA = x1 + lengthX;
            var xB = x2 - lengthX;

            /*
            if (widthX >= 120)
            {
                xA = xB = xC = x2 - 60;
            }
            //*/

            var points = new List<PointF> { 
				new PointF(x1, y1),
				new PointF(xA, y1),
				new PointF(xB, y2),
				new PointF(x2 - extra_thickness, y2)
			};

            var t = 1.0f;//Math.Min(1, Math.Max(0, (widthX - 30) / 60.0f));
            var yA = (yB * t) + (yC * (1 - t));

            // if (widthX <= 120)
            // {
            //     points.Insert(2, new PointF(xB, yA));
            //     points.Insert(2, new PointF(xC, yA));
            //     points.Insert(2, new PointF(xA, yA));
            // }
            //*
            using (var tempPath = new GraphicsPath())
            {
                tempPath.AddBeziers(points.ToArray());
                tempPath.Flatten();
                points = tempPath.PathPoints.ToList();
            }
            //*/
            var angles = new PointF[points.Count - 1];
            var lengths = new float[points.Count - 1];
            float totalLength = 0;
            centerX = 0;
            centerY = 0;
            points.Add(points[points.Count - 1]);
            for (int i = 0; i < points.Count - 2; i++)
            {
                var pt1 = points[i];
                var pt2 = points[i + 1];
                var pt3 = points[i + 2];
                var deltaX = (float)((pt2.X - pt1.X) + (pt3.X - pt2.X));
                var deltaY = (float)((pt2.Y - pt1.Y) + (pt3.Y - pt2.Y));
                var length = (float)Math.Sqrt((deltaX * deltaX) + (deltaY * deltaY));
                if (length <= 1.0f)
                {
                    points.RemoveAt(i);
                    i--;
                    continue;
                }
                lengths[i] = length;
                totalLength += length;
                angles[i].X = deltaX / length;
                angles[i].Y = deltaY / length;
            }

            float midLength = (totalLength / 2.0f);// * 0.75f;
            float startWidth = extra_thickness + GraphConstants.ConnectionWidth;
            float endWidth = extra_thickness + GraphConstants.ConnectionWidth;
            float currentLength = 0;
            var newPoints = new List<PointF>();

            if (points.Count!=0)
            {
                var angle = angles[0]; 
                var point = points[0]; 
                var width = (((currentLength * (endWidth - startWidth)) / totalLength) + startWidth);
                var angleX = angle.X * width;
                var angleY = angle.Y * width;
                newPoints.Add(new PointF(point.X - angleY, point.Y + angleX));
                newPoints.Insert(0, new PointF(point.X + angleY, point.Y - angleX));
            }

            for (int i = 0; i < points.Count - 3; i++)
            {
                var angle = angles[i];
                var point = points[i + 1];
                var length = lengths[i];
                var width = (((currentLength * (endWidth - startWidth)) / totalLength) + startWidth);
                var angleX = angle.X * width;
                var angleY = angle.Y * width;

                var newLength = currentLength + length;
                if (currentLength <= midLength &&
                    newLength >= midLength)
                {
                    var dX = point.X - points[i].X;
                    var dY = point.Y - points[i].Y;
                    var t1 = midLength - currentLength;
                    var l = length;
                    centerX = points[i].X + ((dX * t1) / l);
                    centerY = points[i].Y + ((dY * t1) / l);
                }

                var pt1 = new PointF(point.X - angleY, point.Y + angleX);
                var pt2 = new PointF(point.X + angleY, point.Y - angleX);
                if (newPoints.Count == 0)
                {
                    newPoints.Add(pt1);
                    newPoints.Insert(0, pt2);
                }
                else
                {
                    if (Math.Abs(newPoints[newPoints.Count - 1].X - pt1.X) > 1.0f ||
                        Math.Abs(newPoints[newPoints.Count - 1].Y - pt1.Y) > 1.0f)
                        newPoints.Add(pt1);
                    if (Math.Abs(newPoints[0].X - pt2.X) > 1.0f ||
                        Math.Abs(newPoints[0].Y - pt2.Y) > 1.0f)
                        newPoints.Insert(0, pt2);
                }

                currentLength = newLength;
            }

            if (points.Count >= 2)
            {
                newPoints.Add(points[points.Count - 1]);
            }

            return newPoints;
        }

		static GraphicsPath GetArrowLinePath(float x1, float y1, float x2, float y2, out float centerX, out float centerY, bool include_arrow, float extra_thickness = 0)
		{
			var newPoints = GetArrowLinePoints(x1, y1, x2, y2, out centerX, out centerY, extra_thickness);

			var path = new GraphicsPath(FillMode.Winding);
			path.AddLines(newPoints.ToArray());
			if (include_arrow)
				path.AddLines(GetArrowPoints(x2, y2, extra_thickness).ToArray());
			path.CloseFigure();
			return path;
		}

        private static bool IsInputConnector(NodeConnector connector)
        {
            return connector.Node.InputConnectors.Contains(connector);
        }

		public static void RenderConnection(Graphics graphics, NodeConnector connector, float x, float y, RenderState state)
		{
			if (graphics == null ||
                connector == null)
				return;

            bool isInput = IsInputConnector(connector);
            var connectorInterface = ConnectorInterfacePoint(connector, isInput);
			
			float startX, startY;
            float endX, endY;
            if (isInput)
            {
                startX = x; startY = y;
                endX = connectorInterface.X;
                endY = connectorInterface.Y;
            }
            else
            {
                startX = connectorInterface.X;
                startY = connectorInterface.Y;
                endX = x; endY = y;
            }

            float centerX, centerY;
            using (var path = GetArrowLinePath(startX, startY, endX, endY, out centerX, out centerY, true, 0.0f))
			{
				using (var brush = new SolidBrush(GetArrowLineColor(state)))
				{
					graphics.FillPath(brush, path);
                    graphics.DrawPath(ConnectionBorderPen, path);
				}
			}
		}
		
		public static GraphicsPath CreateRoundedRectangle(SizeF size, PointF location)
		{
			int cornerSize			= (int)GraphConstants.CornerSize * 2;

			var height				= size.Height;
			var width				= size.Width;
			var halfWidth			= width / 2.0f;
			var halfHeight			= height / 2.0f;
			var left				= location.X;
			var top					= location.Y;
			var right				= location.X + width;
			var bottom				= location.Y + height;

			var path = new GraphicsPath(FillMode.Winding);
			path.AddArc(left, top, cornerSize, cornerSize, 180, 90);
			path.AddArc(right - cornerSize, top, cornerSize, cornerSize, 270, 90);

			path.AddArc(right - cornerSize, bottom - cornerSize, cornerSize, cornerSize, 0, 90);
			path.AddArc(left, bottom - cornerSize, cornerSize, cornerSize, 90, 90);
			path.CloseFigure();
			return path;
		}

        // static for good performance
        private static Image shadowDownRight     = HyperGraph.Properties.Resources.tshadowdownright;
        private static Image shadowDownLeft      = HyperGraph.Properties.Resources.tshadowdownleft;
        private static Image shadowDown          = HyperGraph.Properties.Resources.tshadowdown;
        private static Image shadowRight         = HyperGraph.Properties.Resources.tshadowright;
        private static Image shadowTopRight      = HyperGraph.Properties.Resources.tshadowtopright;

        private static void DrawShadow(Graphics context, Rectangle originalRect)
        {
            const int shadowSize = 5;
            const int shadowMargin = 2;

            // Create tiled brushes for the shadow on the right and at the bottom.
            var shadowRightBrush = new TextureBrush(shadowRight, WrapMode.Tile);
            var shadowDownBrush = new TextureBrush(shadowDown, WrapMode.Tile);   

            // Translate (move) the brushes so the top or left of the image matches the top or left of the
            // area where it's drawn. If you don't understand why this is necessary, comment it out. 
            // Hint: The tiling would start at 0,0 of the control, so the shadows will be offset a little.
            // shadowDownBrush.TranslateTransform(0, originalRect.Height - shadowSize);
            // shadowRightBrush.TranslateTransform(originalRect.Width - shadowSize, 0);
            shadowDownBrush.TranslateTransform(0, originalRect.Bottom);
            shadowRightBrush.TranslateTransform(originalRect.Right, 0);

            // Define the rectangles that will be filled with the brush.
            // (where the shadow is drawn)
            Rectangle shadowDownRectangle = new Rectangle(
                originalRect.X + shadowSize + shadowMargin,                    // X
                originalRect.Bottom,                                           // Y
                originalRect.Width - (shadowSize * 2 + shadowMargin),          // width (stretches)
                shadowSize                                                     // height
                );

            Rectangle shadowRightRectangle = new Rectangle(
                originalRect.Right,                                            // X
                originalRect.Y + shadowSize + shadowMargin,                    // Y
                shadowSize,                                                    // width
                originalRect.Height - (shadowSize * 2 + shadowMargin)          // height (stretches)
                );

            // And draw the shadow on the right and at the bottom.
            context.FillRectangle(shadowDownBrush, shadowDownRectangle);
            context.FillRectangle(shadowRightBrush, shadowRightRectangle);

            // Now for the corners, draw the 3 5X5 pixel images.
            context.DrawImage( shadowTopRight,  new Rectangle(  originalRect.Right, originalRect.Y + shadowMargin,  shadowSize, shadowSize  ));
            context.DrawImage( shadowDownRight, new Rectangle(  originalRect.Right, originalRect.Bottom,            shadowSize, shadowSize  ));
            context.DrawImage( shadowDownLeft,  new Rectangle(  originalRect.X + shadowMargin, originalRect.Bottom, shadowSize, shadowSize  ));
        }
	}
}
