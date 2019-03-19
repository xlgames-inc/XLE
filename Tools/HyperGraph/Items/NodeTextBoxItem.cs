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

		public NodeTextBoxItem(string text)
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
                    this.TextSize.Height = Math.Max(size.Height, this.TextSize.Height);
				}
				return this.TextSize;
			} else
			{
				return new SizeF(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
			}
		}

        private static Brush BackgroundBrush = new SolidBrush(Color.FromArgb(96, 96, 96));

        public override void Render(Graphics graphics, RectangleF boundary, object context)
		{
			var path = GraphRenderer.CreateRoundedRectangle(boundary.Size, boundary.Location);

            RectangleF textBoundary = boundary;

			if ((state & RenderState.Hover) == RenderState.Hover)
			{
                graphics.DrawPath(Pens.LightGray, path);
                graphics.FillPath(BackgroundBrush, path);
                graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, textBoundary, GraphConstants.LeftTextStringFormat);
			}
            else
			{
                // graphics.DrawPath(Pens.Black, path);
                graphics.FillPath(BackgroundBrush, path);
                graphics.DrawString(this.Text, SystemFonts.MenuFont, Brushes.Black, textBoundary, GraphConstants.LeftTextStringFormat);
			}
		}
	}
}
