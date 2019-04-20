#region License
// Copyright (c) 2009 Sander van Rossen
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
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace HyperGraph.Items
{
	public sealed class NodeColorItem : NodeItem
	{
		public event EventHandler<NodeItemEventArgs> Clicked;

		public NodeColorItem(string text, Color color)
		{
			this.Text = text;
			this.Color = color;
		}

		public Color Color { get; set; }

		#region Text
		string internalText = string.Empty;
		public string Text
		{
			get { return internalText; }
			set
			{
				if (internalText == value)
					return;
				internalText = value;
				TextSize = Size.Empty;
			}
		}
		#endregion

		internal SizeF TextSize;

        public override bool OnClick(System.Windows.Forms.Control container, System.Windows.Forms.MouseEventArgs evnt, System.Drawing.Drawing2D.Matrix viewTransform)
		{
			base.OnClick(container, evnt, viewTransform);
			if (Clicked != null)
				Clicked(this, new NodeItemEventArgs(this));
			return true;
		}

		
		const int ColorBoxSize = 16;
		const int Spacing = 2;

        public override SizeF Measure(Graphics graphics, object context)
		{
			if (!string.IsNullOrWhiteSpace(this.Text))
			{
				if (this.TextSize.IsEmpty)
				{
					var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
					this.TextSize = graphics.MeasureString(this.Text, SystemFonts.MenuFont, size, GraphConstants.CenterMeasureTextStringFormat);
					this.TextSize.Width  = Math.Max(size.Width, this.TextSize.Width + ColorBoxSize + Spacing);
					this.TextSize.Height = Math.Max(size.Height, this.TextSize.Height);
				}
				return this.TextSize;
			} else
			{
				return new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.TitleHeight + GraphConstants.TopHeight);
			}
		}

        public override void Render(Graphics graphics, RectangleF boundary, object context)
		{
			var alignment	= HorizontalAlignment.Center;
			var format		= GraphConstants.CenterTextStringFormat;
			var rect		= boundary;
            var colorBox	= boundary;
			colorBox.Width	= ColorBoxSize;
			switch (alignment)
			{
				case HorizontalAlignment.Left:
					rect.Width	-= ColorBoxSize + Spacing;
					rect.X		+= ColorBoxSize + Spacing;
					break;
				case HorizontalAlignment.Right:
					colorBox.X	= rect.Right - colorBox.Width;
					rect.Width	-= ColorBoxSize + Spacing;
					break;
				case HorizontalAlignment.Center:
					rect.Width	-= ColorBoxSize + Spacing;
					rect.X		+= ColorBoxSize + Spacing;
					break;
			}

			graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, rect, format);

			using (var path = GraphRenderer.CreateRoundedRectangle(colorBox.Size, colorBox.Location))
			{
				using (var brush = new SolidBrush(this.Color))
				{
					graphics.FillPath(brush, path);
				}
				if ((state & RenderState.Hover) != 0)
					graphics.DrawPath(Pens.White, path);
				else
					graphics.DrawPath(Pens.Black, path);
			}
			//using (var brush = new SolidBrush(this.Color))
			//{
			//	graphics.FillRectangle(brush, colorBox.X, colorBox.Y, colorBox.Width, colorBox.Height);
			//}
			//graphics.DrawRectangle(Pens.Black, colorBox.X, colorBox.Y, colorBox.Width, colorBox.Height);
		}
	}
}
