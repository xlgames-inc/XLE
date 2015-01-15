#region License
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
using System.Drawing;

namespace HyperGraph.Items
{
	/// <summary>
	/// An item that contains a slider which displays its value as a text on the slider itself
	/// </summary>
	public sealed class NodeNumericSliderItem : NodeSliderItem
	{
		/// <summary>
		/// Construct a new NodeNumericSliderItem.
		/// </summary>
		/// <param name="text">The label for the item.</param>
		/// <param name="sliderSize">The minimum size the slider should have inside the parent node.</param>
		/// <param name="textSize">The text size.</param>
		/// <param name="minValue">The lowest possible value for the slider.</param>
		/// <param name="maxValue">The highest possible value for the slider.</param>
		/// <param name="defaultValue">The value the slider should start with.</param>
		/// <param name="inputEnabled">Does the item accept an input to be connected?</param>
		/// <param name="outputEnabled">Does the item accept an output to be connected?</param>
		public NodeNumericSliderItem( string text, float sliderSize, float textSize, float minValue, float maxValue, float defaultValue, bool inputEnabled, bool outputEnabled ) : base( text, sliderSize, textSize, minValue, maxValue, defaultValue, inputEnabled, outputEnabled ) {}

		/// <summary>
		/// Render the slider.
		/// </summary>
		/// <param name="graphics">The <see cref="Graphics"/> instance that should be used for drawing.</param>
		/// <param name="minimumSize">The smallest size the slider has to fit into.</param>
		/// <param name="location">Where the slider should be drawn.</param>
        public override void Render(Graphics graphics, SizeF minimumSize, PointF location)
		{
			var size = Measure(graphics);
			size.Width  = Math.Max(minimumSize.Width, size.Width);
			size.Height = Math.Max(minimumSize.Height, size.Height);

			var sliderOffset	= Spacing + this.textSize.Width;
			var sliderWidth		= size.Width - sliderOffset	;

			var textRect	= new RectangleF(location, size);
			var sliderBox	= new RectangleF(location, size);
			var sliderRect	= new RectangleF(location, size);
			
			// Calculate bounds for outer rectangle
			sliderRect.X		= sliderRect.Right - sliderWidth;
			sliderRect.Y		+= ((sliderRect.Bottom - sliderRect.Top) - SliderHeight) / 2.0f;
			sliderRect.Width	= sliderWidth;
			sliderRect.Height	= SliderHeight;
			
			textRect.Width -= sliderWidth + Spacing;

			var valueSize		= (this.MaxValue - this.MinValue);
			this.sliderRect		= sliderRect;
			this.sliderRect.X	+= SliderBoxSize / 2.0f;

			// Calculate bounds for inner rectangle
			sliderBox.X			= sliderRect.X;
			sliderBox.Y			= sliderRect.Y;
			sliderBox.Width		= (this.Value * this.sliderRect.Width) / valueSize;
			sliderBox.Height	= SliderHeight;

			// Draw label
			graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, textRect, GraphConstants.LeftTextStringFormat);
			
			// Draw inner rectangle
			graphics.FillRectangle(Brushes.LightGray, sliderBox.X, sliderBox.Y, sliderBox.Width, sliderBox.Height);

			// Draw outer rectangle
			if ((state & (RenderState.Hover | RenderState.Dragging)) != 0)
				graphics.DrawRectangle(Pens.White, sliderRect.X, sliderRect.Y, sliderRect.Width, sliderRect.Height);
			else
				graphics.DrawRectangle(Pens.Black, sliderRect.X, sliderRect.Y, sliderRect.Width, sliderRect.Height);

			// Draw value marker into box
			if ((state & (RenderState.Hover | RenderState.Dragging)) != 0)
				graphics.DrawLine(Pens.White, sliderBox.X + sliderBox.Width, sliderBox.Y, sliderBox.X + sliderBox.Width, sliderBox.Y + sliderBox.Height);
			else
				graphics.DrawLine(Pens.Black, sliderBox.X + sliderBox.Width, sliderBox.Y, sliderBox.X + sliderBox.Width, sliderBox.Y + sliderBox.Height);

			// Draw value
			graphics.DrawString(this.Value.ToString(), SystemFonts.MenuFont, Brushes.Black, sliderRect, GraphConstants.LeftTextStringFormat);
		}
	}
}
