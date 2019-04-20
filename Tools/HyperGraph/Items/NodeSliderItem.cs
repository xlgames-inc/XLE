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
	public class NodeSliderItem : NodeItem
	{
		public event EventHandler<NodeItemEventArgs> Clicked;
		public event EventHandler<NodeItemEventArgs> ValueChanged;

		public NodeSliderItem(string text, float sliderSize, float textSize, float minValue, float maxValue, float defaultValue)
		{
			this.Text = text;
			this.MinimumSliderSize = sliderSize;
			this.TextSize = textSize;
			this.MinValue = Math.Min(minValue, maxValue);
			this.MaxValue = Math.Max(minValue, maxValue);
			this.Value = defaultValue;
		}

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
				itemSize = Size.Empty;
			}
		}
		#endregion

		#region Dragging
		internal bool Dragging { get; set; }
		#endregion

		public float MinimumSliderSize	{ get; set; }
		public float TextSize			{ get; set; }

		public float MinValue { get; set; }
		public float MaxValue { get; set; }

		float internalValue = 0.0f;
		public float Value				
		{
			get { return internalValue; }
			set
			{
				var newValue = value;
				if (newValue < MinValue) newValue = MinValue;
				if (newValue > MaxValue) newValue = MaxValue;
				if (internalValue == newValue)
					return;
				internalValue = newValue;
				if (ValueChanged != null)
					ValueChanged(this, new NodeItemEventArgs(this));
			}
		}


        public override bool OnClick(System.Windows.Forms.Control container, System.Windows.Forms.MouseEventArgs evnt, System.Drawing.Drawing2D.Matrix viewTransform)
		{
            base.OnClick(container, evnt, viewTransform);
			if (Clicked != null)
				Clicked(this, new NodeItemEventArgs(this));
			return true;
		}

		public override bool OnStartDrag(PointF location, out PointF original_location) 
		{
			base.OnStartDrag(location, out original_location);
			var size = (MaxValue - MinValue);
			original_location.Y = location.Y;
			original_location.X = ((Value / size) * sliderRect.Width) + sliderRect.Left;
			Value = ((location.X - sliderRect.Left) / sliderRect.Width) * size;
			Dragging = true; 
			return true; 
		}

		public override bool OnDrag(PointF location) 
		{
			base.OnDrag(location);
			var size = (MaxValue - MinValue);
			Value = ((location.X - sliderRect.Left) / sliderRect.Width) * size;
			return true; 
		}

		public override bool OnEndDrag() { base.OnEndDrag(); Dragging = false; return true; }


		internal SizeF itemSize;
		internal SizeF textSize;
		internal RectangleF sliderRect;


		protected const int SliderBoxSize = 4;
		protected const int SliderHeight	= 8;
		protected const int Spacing		= 2;

		public override SizeF Measure(Graphics graphics, object context)
		{
			if (!string.IsNullOrWhiteSpace(this.Text))
			{
				if (this.itemSize.IsEmpty)
				{
					var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
					var sliderWidth = this.MinimumSliderSize + SliderBoxSize;

					this.textSize			= (SizeF)graphics.MeasureString(this.Text, SystemFonts.MenuFont, size, GraphConstants.LeftMeasureTextStringFormat);
					this.textSize.Width		= Math.Max(this.TextSize, this.textSize.Width + 4);
					this.itemSize.Width		= Math.Max(size.Width, this.textSize.Width + sliderWidth + Spacing);
					this.itemSize.Height	= Math.Max(size.Height, this.textSize.Height);
				}
				return this.itemSize;
			} else
			{
				return new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
			}
		}

        public override void Render(Graphics graphics, RectangleF boundary, object context)
		{
			var sliderOffset	= Spacing + this.textSize.Width;
			var sliderWidth		= boundary.Width - (Spacing + this.textSize.Width);

			var textRect	= boundary;
			var sliderBox	= boundary;
			var sliderRect	= boundary;
			sliderRect.X =		 sliderRect.Right - sliderWidth;
			sliderRect.Y		+= ((sliderRect.Bottom - sliderRect.Top) - SliderHeight) / 2.0f;
			sliderRect.Width	= sliderWidth;
			sliderRect.Height	= SliderHeight;
			textRect.Width -= sliderWidth + Spacing;

			var valueSize = (this.MaxValue - this.MinValue);
			this.sliderRect = sliderRect;
			this.sliderRect.Width -= SliderBoxSize;
			this.sliderRect.X += SliderBoxSize / 2.0f;

			sliderBox.Width = SliderBoxSize;
			sliderBox.X = sliderRect.X + (this.Value * this.sliderRect.Width) / valueSize;

			graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, textRect, GraphConstants.LeftTextStringFormat);

			using (var path = GraphRenderer.CreateRoundedRectangle(sliderRect.Size, sliderRect.Location))
			{
				if ((state & (RenderState.Hover | RenderState.Dragging)) != 0)
					graphics.DrawPath(Pens.White, path);
				else
					graphics.DrawPath(Pens.Black, path);
			}

			graphics.FillRectangle(Brushes.LightGray, sliderBox.X, sliderBox.Y, sliderBox.Width, sliderBox.Height);

			if ((state & (RenderState.Hover | RenderState.Dragging)) != 0)
				graphics.DrawRectangle(Pens.White, sliderBox.X, sliderBox.Y, sliderBox.Width, sliderBox.Height);
			else
				graphics.DrawRectangle(Pens.Black, sliderBox.X, sliderBox.Y, sliderBox.Width, sliderBox.Height);
		}
	}
}
