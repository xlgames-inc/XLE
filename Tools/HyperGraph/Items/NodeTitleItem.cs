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

namespace HyperGraph.Items
{
	public sealed class NodeTitleItem : NodeItem
	{
		public string	Title { get; set; }

        public override SizeF Measure(Graphics graphics, object context)
		{
			if (!string.IsNullOrWhiteSpace(this.Title))
			{
				var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.TitleHeight);
				var texSize = graphics.MeasureString(this.Title, GraphConstants.TitleFont, size, GraphConstants.TitleMeasureStringFormat);

                texSize.Width   = Math.Max(size.Width, texSize.Width + (GraphConstants.CornerSize * 2));
                texSize.Height	= Math.Max(size.Height, texSize.Height);
				return texSize;
			} else
			{
				return new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.TitleHeight);
			}
		}

        private static Brush BackgroundBrush = new SolidBrush(Color.FromArgb(96, 96, 96));

        public override void Render(Graphics graphics, RectangleF boundary, object context)
		{
            if (Node.Layout == Node.LayoutType.Circular)
            {
                var path = GraphRenderer.CreateRoundedRectangle(boundary.Size, boundary.Location);
                graphics.FillPath(BackgroundBrush, path);
            }

            if ((state & RenderState.Hover) == RenderState.Hover)
				graphics.DrawString(this.Title, GraphConstants.TitleFont, Brushes.White, boundary, GraphConstants.TitleStringFormat);
			else
                graphics.DrawString(this.Title, GraphConstants.TitleFont, Brushes.LightGray, boundary, GraphConstants.TitleStringFormat);
		}
	}
}
