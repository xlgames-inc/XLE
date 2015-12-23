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
using System.ComponentModel;

namespace HyperGraph.Items
{
	public sealed class AcceptNodeTextChangedEventArgs : CancelEventArgs
	{
		public AcceptNodeTextChangedEventArgs(string old_text, string new_text) { PreviousText = old_text; Text = new_text; }
		public AcceptNodeTextChangedEventArgs(string old_text, string new_text, bool cancel) : base(cancel) { PreviousText = old_text; Text = new_text; }
		public string			PreviousText	{ get; private set; }
		public string			Text			{ get; set; }
	}

	public sealed class NodeTextBoxItem : NodeItem
	{
		public event EventHandler<AcceptNodeTextChangedEventArgs> TextChanged;

		public NodeTextBoxItem(string text, bool inputEnabled, bool outputEnabled) :
			base(inputEnabled, outputEnabled)
		{
			this.Text = text;
		}

		public NodeTextBoxItem(string text) :
			this(text, false, false)
		{
			this.Text = text;
		}

		#region Name
		public string Name
		{
			get;
			set;
		}
		#endregion

		#region Text
		string internalText = string.Empty;
		public string Text
		{
			get { return internalText; }
			set
			{
				if (internalText == value)
					return;
				if (TextChanged != null)
				{
					var eventArgs = new AcceptNodeTextChangedEventArgs(internalText, value);
					TextChanged(this, eventArgs);
					if (eventArgs.Cancel)
						return;
					internalText = eventArgs.Text;
				} else
					internalText = value;
				TextSize = Size.Empty;
			}
		}
		#endregion

		internal SizeF TextSize;

        public override bool OnDoubleClick(System.Windows.Forms.Control container)
		{
            base.OnDoubleClick(container);
			var form = new TextEditForm();
			form.Text = Name ?? "Edit text";
			form.InputText = Text;
			var result = form.ShowDialog();
			if (result == DialogResult.OK)
				Text = form.InputText;
			return true;
		}

        public override SizeF Measure(Graphics graphics)
		{
			if (!string.IsNullOrWhiteSpace(this.Text))
			{
				if (this.TextSize.IsEmpty)
				{
					var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);

					this.TextSize = graphics.MeasureString(this.Text, SystemFonts.MenuFont, size, GraphConstants.LeftMeasureTextStringFormat);
					
					this.TextSize.Width  = Math.Max(size.Width, this.TextSize.Width + 8);
					this.TextSize.Height = Math.Max(size.Height, this.TextSize.Height + 2);
				}
				return this.TextSize;
			} else
			{
				return new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
			}
		}

        public override void Render(Graphics graphics, SizeF minimumSize, PointF location)
		{
			var size = Measure(graphics);
			size.Width  = Math.Max(minimumSize.Width, size.Width);
			size.Height = Math.Max(minimumSize.Height, size.Height);

			var path = GraphRenderer.CreateRoundedRectangle(size, location);

			location.Y += 1;
			location.X += 1;

			if ((state & RenderState.Hover) == RenderState.Hover)
			{
				graphics.DrawPath(Pens.White, path);
				graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, new RectangleF(location, size), GraphConstants.LeftTextStringFormat);
			} else
			{
				graphics.DrawPath(Pens.Black, path);
				graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, new RectangleF(location, size), GraphConstants.LeftTextStringFormat);
			}
		}

        public override void RenderConnector(Graphics graphics, RectangleF rectangle) { }
	}
}
