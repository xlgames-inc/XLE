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
		static IEnumerable<NodeItem> EnumerateNodeItems(Node node)
		{
			if (node == null)
				yield break;

			yield return node.titleItem;
			if (node.Collapsed)
				yield break;
			
			foreach (var item in node.Items)
				yield return item;
		}

        public static uint GetSide(NodeItem item)
        {
            if (item.Input != null && item.Input.Enabled) return 0;
            else if (item.Output != null && item.Output.Enabled) return 2;
            return 1;
        }

        private struct NodeSize
        {
            internal SizeF BaseSize { get; set; }
            internal float LeftPartWidth { get; set; }
            internal float RightPartWidth { get; set; }
        };

        private static NodeSize Measure(Graphics context, Node node)
        {
            if (node == null)
                return new NodeSize { BaseSize = SizeF.Empty, LeftPartWidth = 0, RightPartWidth = 0 };

            SizeF size = Size.Empty;
            size.Height = (int)GraphConstants.TopHeight;

            bool[] firstItem = new bool[3] { true, true, true };
            SizeF[] sizes = new SizeF[3] { size, size, size };
            foreach (var item in EnumerateNodeItems(node))
            {
                var side = GetSide(item);
                var itemSize = item.Measure(context);

                if (side != 1)
                    itemSize = AdjustConnectorSize(itemSize);

                sizes[side].Width = Math.Max(sizes[side].Width, itemSize.Width);
                if (!firstItem[side])
                    sizes[side].Height += GraphConstants.ItemSpacing;
                sizes[side].Height += itemSize.Height;
                firstItem[side] = false;
            }

            size = sizes[1];
            for (uint c = 0; c < 3; ++c)
                size.Height = Math.Max(size.Height, sizes[c].Height);

            size.Width += GraphConstants.NodeExtraWidth;
            size.Height += GraphConstants.BottomHeight;

            return new NodeSize { BaseSize = size, LeftPartWidth = sizes[0].Width, RightPartWidth = sizes[2].Width };
        }

        static SizeF PreRenderItem(Graphics graphics, NodeItem item, PointF position)
		{
			var itemSize = item.Measure(graphics);
			item.bounds = new RectangleF(position, itemSize);
			return itemSize;
		}

		static void RenderItem(Graphics graphics, SizeF minimumSize, NodeItem item, PointF position, object context)
		{
			item.Render(graphics, minimumSize, position, context);
		}

		internal static Pen BorderPen = new Pen(Color.FromArgb(200, 200, 200));
        internal static Brush CompatibleBrush = new SolidBrush(Color.FromArgb(255, 196, 196));

        internal static Brush DraggingBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(140, 120, 120),  Color.FromArgb(96, 96, 96));
        internal static Brush HoverBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(80, 80, 80), Color.FromArgb(96, 96, 96));
        internal static Brush NormalBrush = new HatchBrush(HatchStyle.LightDownwardDiagonal,
                                                            Color.FromArgb(120, 120, 120),  Color.FromArgb(96, 96, 96));
        internal static Brush TitleAreaBrush = new SolidBrush(Color.FromArgb(96, 96, 96));

        internal static Pen FocusPen = new Pen(Color.FromArgb(255, 255, 255), 3.0f);

        enum ConnectorType { Input, Output };

		static void RenderConnector(Graphics graphics, RectangleF bounds, RenderState state, ConnectorType type, NodeConnector connector=null)
		{
            var cornerSize = GraphConstants.ConnectorCornerSize;

			using (var brush = new SolidBrush(GetArrowLineColor(state)))
			{
                using (var path = new GraphicsPath(FillMode.Winding))
                {
                    Rectangle statusRect;
                    RectangleF clientRect;

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

                    graphics.FillPath(((state & RenderState.Hover)!=0) ? HoverBrush : NormalBrush , path);
                    graphics.DrawPath(BorderPen, path);
                    graphics.FillRectangle(brush, statusRect);

                    if ((state & (RenderState.Compatible | RenderState.Conversion | RenderState.Dragging)) == RenderState.Compatible)
                    {
                        graphics.FillRectangle(CompatibleBrush, statusRect);
                        graphics.DrawRectangle(BorderPen, statusRect);
                    }
                    else
                    {
                        graphics.FillRectangle(brush, statusRect);
                    }

                    if (connector!=null)
                    {
                        connector.Item.RenderConnector(graphics, clientRect);
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

        private static PointF ConnectorInterfacePoint(RectangleF bounds, ConnectorType type)
        {
            float width = bounds.Bottom - bounds.Top - 8;
            if (type == ConnectorType.Input) {
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

		public static void PerformLayout(Graphics graphics, IEnumerable<Node> nodes)
		{
			foreach (var node in nodes.Reverse<Node>())
			{
				GraphRenderer.PerformLayout(graphics, node);
			}
		}

		public static void Render(Graphics graphics, IEnumerable<Node> nodes, bool showLabels, object context)
		{
			var skipConnections = new HashSet<NodeConnection>();
			foreach (var node in nodes.Reverse<Node>())
			{
				GraphRenderer.RenderConnections(graphics, node, skipConnections, showLabels);
			}
			foreach (var node in nodes.Reverse<Node>())
			{
				GraphRenderer.Render(graphics, node, context);
			}
		}

		public static void PerformLayout(Graphics graphics, Node node)
		{
			if (node == null)
				return;
			var size = Measure(graphics, node);
			var position = node.Location;
            node.bounds = new RectangleF(position, size.BaseSize);
            position.Y += (int)GraphConstants.TopHeight;
			
			var path = new GraphicsPath(FillMode.Winding);
			var left = position.X;
			var top = position.Y;
            var right = position.X + size.BaseSize.Width;
            var bottom = position.Y + size.BaseSize.Height;
			
			node.inputConnectors.Clear();
			node.outputConnectors.Clear();
			//node.connections.Clear();

			var itemPosition = position;
			itemPosition.X += (int)GraphConstants.HorizontalSpacing;
			if (node.Collapsed)
			{
				foreach (var item in node.Items)
				{
					var inputConnector	= item.Input;
					if (inputConnector != null && inputConnector.Enabled)
					{
						inputConnector.bounds = Rectangle.Empty;
						node.inputConnectors.Add(inputConnector);
					}
					var outputConnector = item.Output;
					if (outputConnector != null && outputConnector.Enabled)
					{
						outputConnector.bounds = Rectangle.Empty;
						node.outputConnectors.Add(outputConnector);
					}
				}
				var itemSize = PreRenderItem(graphics, node.titleItem, itemPosition);
                var connectorY = itemPosition.Y;

                node.inputBounds = new RectangleF(      
                    left - GraphConstants.ConnectorWidthCollapsed, connectorY,
                    GraphConstants.ConnectorWidthCollapsed, itemSize.Height);
                node.outputBounds = new RectangleF(     
                    right - GraphConstants.ConnectorWidthCollapsed, connectorY,
                    GraphConstants.ConnectorWidthCollapsed, itemSize.Height);
			} 
            else
			{
				node.inputBounds	= Rectangle.Empty;
				node.outputBounds	= Rectangle.Empty;

                PointF[] positions = new PointF[] { itemPosition, itemPosition, itemPosition };
                float[] widths = new float[] { size.LeftPartWidth, size.BaseSize.Width, size.RightPartWidth };

				foreach (var item in EnumerateNodeItems(node))
				{
                    var inputConnector = item.Input;
                    var outputConnector = item.Output;

                    var side = GetSide(item);
                    var itemSize = PreRenderItem(graphics, item, positions[side]);
					var realHeight = itemSize.Height;
					
					if (inputConnector != null && inputConnector.Enabled)
					{
						if (itemSize.IsEmpty)
						{
							inputConnector.bounds = Rectangle.Empty;
						} 
                        else
						{
                            inputConnector.bounds = new RectangleF(
                                left - widths[side], positions[side].Y,
                                widths[side], realHeight);
						}
						node.inputConnectors.Add(inputConnector);
					}

					if (outputConnector != null && outputConnector.Enabled)
					{
						if (itemSize.IsEmpty)
						{
							outputConnector.bounds = Rectangle.Empty;
						} 
                        else
						{
                            outputConnector.bounds = new RectangleF(
                                right, positions[side].Y,
                                widths[side], realHeight);
						}
						node.outputConnectors.Add(outputConnector);
					}

                    positions[side].Y += itemSize.Height + GraphConstants.ItemSpacing;
				}
			}
		}
        

		static void Render(Graphics graphics, Node node, object context)
		{
			var size		= node.bounds.Size;
			var position	= node.bounds.Location;

            var cornerSize          = GraphConstants.CornerSize;
			var left				= position.X;
			var top					= position.Y;
			var right				= position.X + size.Width;
			var bottom				= position.Y + size.Height;

            Brush brush;
            if ((node.state & RenderState.Dragging) != 0)       { brush = DraggingBrush; }
            else if ((node.state & RenderState.Hover) != 0)     { brush = HoverBrush; }
            else                                                { brush = NormalBrush; }

            const Boolean roundedRectRendering = false;
            if (roundedRectRendering)
            {
                using (var path = new GraphicsPath(FillMode.Winding))
                {
                    path.AddArc(left, top, cornerSize, cornerSize, 180, 90);
                    path.AddArc(right - cornerSize, top, cornerSize, cornerSize, 270, 90);

                    path.AddArc(right - cornerSize, bottom - cornerSize, cornerSize, cornerSize, 0, 90);
                    path.AddArc(left, bottom - cornerSize, cornerSize, cornerSize, 90, 90);
                    path.CloseFigure();

                    graphics.FillPath(brush, path);
                    graphics.DrawPath(BorderPen, path);
                }
            }
            else 
            {
                var rect = new Rectangle((int)left, (int)top, (int)(right - left), (int)(bottom - top));

                if (node.Collapsed)
                {
                    graphics.FillRectangle(TitleAreaBrush, rect);
                }
                else
                {
                    int titleHeight = (node.titleItem != null) ? (int)(node.titleItem.bounds.Height + GraphConstants.TopHeight) : 0;
                    titleHeight = Math.Min((int)(bottom - top), titleHeight);

                    var titleRect = new Rectangle((int)left, (int)top, (int)(right - left), titleHeight);
                    var backgroundRect = new Rectangle((int)left, (int)top + titleHeight, (int)(right - left), (int)(bottom - top) - titleHeight);

                    DrawShadow(graphics, rect);
                    graphics.FillRectangle(brush, backgroundRect);
                    if (titleHeight != 0)
                        graphics.FillRectangle(TitleAreaBrush, titleRect);
                    graphics.DrawRectangle(BorderPen, rect);
                }

                if ((node.state & RenderState.Focus) != 0)
                {
                    // We're going to draw a outline around the edge of the entire node
                    // This should be a thick line, with some bright color
                    var outline = new Rectangle(rect.Left - 4, rect.Top - 4, rect.Width + 8, rect.Height + 8);
                    graphics.DrawRectangle(FocusPen, outline);
                }
            }

			/*
			    if (!node.Collapsed)
				    graphics.DrawLine(Pens.Black, 
					    left  + GraphConstants.ConnectorSize, node.titleItem.bounds.Bottom - GraphConstants.ItemSpacing, 
					    right - GraphConstants.ConnectorSize, node.titleItem.bounds.Bottom - GraphConstants.ItemSpacing);
			*/

			if (node.Collapsed)
			{
				// bool inputConnected = false;
				// var inputState	= RenderState.None;
				// var outputState = node.outputState;
				// foreach (var connection in node.connections)
				// {
				// 	if (connection.To.Node == node)
				// 	{
				// 		inputState |= connection.state;
				// 		inputConnected = true;
				// 	}
				// 	if (connection.From.Node == node)
				// 		outputState |= connection.state | RenderState.Connected;
				// }

				RenderItem(graphics, new SizeF(node.bounds.Width - GraphConstants.NodeExtraWidth, 0), node.titleItem, node.titleItem.bounds.Location, context);
				// if (node.inputConnectors.Count > 0)
				// 	RenderConnector(graphics, node.inputBounds, node.inputState, ConnectorType.Input);
				// if (node.outputConnectors.Count > 0)
				// 	RenderConnector(graphics, node.outputBounds, outputState, ConnectorType.Output);
				//if (inputConnected)
				//	RenderArrow(graphics, node.inputBounds, inputState);
			} 
            else
			{
				node.inputBounds	= Rectangle.Empty;
				node.outputBounds	= Rectangle.Empty;
				
				var minimumItemSize = new SizeF(node.bounds.Width - GraphConstants.NodeExtraWidth, 0);
				foreach (var item in EnumerateNodeItems(node))
				{
					RenderItem(graphics, minimumItemSize, item, item.bounds.Location, context);
					var inputConnector	= item.Input;
					if (inputConnector != null && inputConnector.Enabled)
					{
						if (!inputConnector.bounds.IsEmpty)
						{
							var state		= RenderState.None;
							var connected	= false;
							foreach (var connection in node.connections)
							{
								if (connection.To == inputConnector)
								{
									state |= connection.state;
									connected = true;
								}
							}

							RenderConnector(graphics, 
											inputConnector.bounds,
											inputConnector.state, ConnectorType.Input,
                                            inputConnector);

							if (connected)
								RenderArrow(graphics, inputConnector.bounds, state);
						}
					}
					var outputConnector = item.Output;
					if (outputConnector != null && outputConnector.Enabled)
					{
						if (!outputConnector.bounds.IsEmpty)
						{
							var state = outputConnector.state;
							foreach (var connection in node.connections)
							{
								if (connection.From == outputConnector)
									state |= connection.state | RenderState.Connected;
							}
                            RenderConnector(graphics, outputConnector.bounds, state, ConnectorType.Output, outputConnector);
						}
					}
				}
			}
		}

		public static void RenderConnections(Graphics graphics, Node node, HashSet<NodeConnection> skipConnections, bool showLabels)
		{
			foreach (var connection in node.connections.Reverse<NodeConnection>())
			{
				if (connection == null)
					continue;

				if (skipConnections.Add(connection))
				{
					var to		= connection.To;
					var from	= connection.From;

                    RectangleF toBounds;
                    RectangleF fromBounds;
                    if (to == null) toBounds = new RectangleF(0.0f, 0.0f, 0.0f, 0.0f);
                    else if (to.Node.Collapsed) toBounds = to.Node.inputBounds;
                    else toBounds = to.bounds;
                    if (from == null) fromBounds = new RectangleF(0.0f, 0.0f, 0.0f, 0.0f);
                    else if (from.Node.Collapsed) fromBounds = from.Node.outputBounds;
                    else fromBounds = from.bounds;

                    var pt1 = ConnectorInterfacePoint(fromBounds, ConnectorType.Output);
                    var pt2 = ConnectorInterfacePoint(toBounds, ConnectorType.Input);

                    if (to != null && from != null)
                    {
                        float centerX;
                        float centerY;
                        using (var path = GetArrowLinePath(pt1.X, pt1.Y, pt2.X, pt2.Y, out centerX, out centerY, false))
                        {
                            using (var brush = new SolidBrush(GetArrowLineColor(connection.state | RenderState.Connected)))
                            {
                                graphics.FillPath(brush, path);
                                graphics.DrawPath(BorderPen, path);
                            }
                            connection.bounds = path.GetBounds();
                        }

                        if (showLabels &&
                            !string.IsNullOrWhiteSpace(connection.Name))
                        {
                            var center = new PointF(centerX, centerY);
                            RenderLabel(graphics, connection, connection.Name, center, connection.state);
                        }
                    }
                    else if (to!=null || from!=null)
                    {
                        //  this is a basic connection. It just connects one connector to a 
                        //  static string value (either a variable name or constant value)
                        //      Draw a small arrow from the connection label to the connector
                        if (!string.IsNullOrWhiteSpace(connection.Name) && !to.Node.Collapsed)
                        {
                            PointF center;
                            if (to != null) center = pt2;
                            else if (from != null) center = pt1;
                            else center = new PointF();
                            center.X -= 16;
                            RenderLabel(graphics, connection, "= " + connection.Name, center, connection.state);
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
			var to		= connection.To;
			var from	= connection.From;
			RectangleF toBounds;
			RectangleF fromBounds;
			if (to.Node.Collapsed)		toBounds = to.Node.inputBounds;
			else						toBounds = to.bounds;
			if (from.Node.Collapsed)	fromBounds = from.Node.outputBounds;
			else						fromBounds = from.bounds;

            var pt1 = ConnectorInterfacePoint(fromBounds, ConnectorType.Output);
            var pt2 = ConnectorInterfacePoint(toBounds, ConnectorType.Input);

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
                    return Color.BurlyWood;
                }
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
				return Color.White;
			} else
            if ((state & RenderState.Conversion) != 0)
            {
                return Color.ForestGreen;
            }
            else
			if ((state & RenderState.Connected) != 0)
			{
                return Color.SteelBlue;
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
            var lengthX = Math.Max(60, Math.Abs(widthX / 2))
                //+ Math.Max(0, -widthX / 2)
                ;
            var lengthY = 0;// Math.Max(-170, Math.Min(-120.0f, widthX - 120.0f)) + 120.0f; 
            if (widthX < 120)
                lengthX = 60;
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

            if (widthX <= 120)
            {
                points.Insert(2, new PointF(xB, yA));
                points.Insert(2, new PointF(xC, yA));
                points.Insert(2, new PointF(xA, yA));
            }
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
            float startWidth = extra_thickness + 6.0f;
            float endWidth = extra_thickness + 6.0f;
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

		public static void RenderOutputConnection(Graphics graphics, NodeConnector output, float x, float y, RenderState state)
		{
			if (graphics == null ||
				output == null)
				return;
			
			RectangleF outputBounds;
			if (output.Node.Collapsed)	outputBounds = output.Node.outputBounds;
			else						outputBounds = output.bounds;

            var interfacePoint = ConnectorInterfacePoint(outputBounds, ConnectorType.Output);
			
			float centerX;
			float centerY;
            using (var path = GetArrowLinePath(interfacePoint.X, interfacePoint.Y, x, y, out centerX, out centerY, true, 0.0f))
			{
				using (var brush = new SolidBrush(GetArrowLineColor(state)))
				{
					graphics.FillPath(brush, path);
                    graphics.DrawPath(BorderPen, path);
				}
			}
		}
		
		public static void RenderInputConnection(Graphics graphics, NodeConnector input, float x, float y, RenderState state)
		{
			if (graphics == null || 
				input == null)
				return;
			
			RectangleF inputBounds;
			if (input.Node.Collapsed)	inputBounds = input.Node.inputBounds;
			else						inputBounds = input.bounds;

            var interfacePoint = ConnectorInterfacePoint(inputBounds, ConnectorType.Input);

			float centerX;
			float centerY;
            using (var path = GetArrowLinePath(x, y, interfacePoint.X, interfacePoint.Y, out centerX, out centerY, true, 0.0f))
			{
				using (var brush = new SolidBrush(GetArrowLineColor(state)))
				{
					graphics.FillPath(brush, path);
                    graphics.DrawPath(BorderPen, path);
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
