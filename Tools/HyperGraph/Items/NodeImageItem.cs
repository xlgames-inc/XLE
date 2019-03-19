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
	public sealed class NodeImageItem : NodeItem
	{
		public event EventHandler<NodeItemEventArgs> Clicked;

		public NodeImageItem(Image image)
		{
			this.Image = image;
		}

		public NodeImageItem(Image image, int width, int height)
		{
			this.Width = width;
			this.Height = height;
			this.Image = image;
		}

		public int? Width { get; set; }
		public int? Height { get; set; }
		public Image Image { get; set; }

        public override bool OnClick(System.Windows.Forms.Control container, System.Windows.Forms.MouseEventArgs evnt, System.Drawing.Drawing2D.Matrix viewTransform)
		{
			base.OnClick(container, evnt, viewTransform);
			if (Clicked != null)
				Clicked(this, new NodeItemEventArgs(this));
			return true;
		}

        public override SizeF Measure(Graphics graphics)
		{
			if (this.Image != null)
			{
				SizeF size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);

				if (this.Width.HasValue)
					size.Width = Math.Max(size.Width, this.Width.Value);
				else
					size.Width = Math.Max(size.Width, this.Image.Width);

				if (this.Height.HasValue)
					size.Height = Math.Max(size.Height, this.Height.Value);
				else
					size.Height = Math.Max(size.Height, this.Image.Height);
				
				return size;
			} else
			{
				var size = new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
				if (this.Width.HasValue)
					size.Width = Math.Max(size.Width, this.Width.Value);

				if (this.Height.HasValue)
					size.Height = Math.Max(size.Height, this.Height.Value);
				
				return size;
			}
		}

        public override void Render(Graphics graphics, RectangleF boundary, object context)
		{
            var location = boundary.Location;
            var size = boundary.Size;

			if (this.Width.HasValue &&
				size.Width > this.Width.Value)
			{
				location.X += (size.Width - (this.Width.Value)) / 2.0f;
				size.Width = (this.Width.Value);
			}
			var rect = new RectangleF(location, size);

			if (this.Image != null)
				graphics.DrawImage(this.Image, rect);
			
			if ((state & RenderState.Hover) != 0)
				graphics.DrawRectangle(Pens.White, rect.Left, rect.Top, rect.Width, rect.Height);
			else
				graphics.DrawRectangle(Pens.Black, rect.Left, rect.Top, rect.Width, rect.Height);
		}
	}
}
