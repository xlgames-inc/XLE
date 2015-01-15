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
	public sealed class AcceptNodeSelectionChangedEventArgs : CancelEventArgs
	{
		public AcceptNodeSelectionChangedEventArgs(int old_index, int new_index) { PreviousIndex = old_index; Index = new_index; }
		public AcceptNodeSelectionChangedEventArgs(int old_index, int new_index, bool cancel) : base(cancel) { PreviousIndex = old_index; Index = new_index; }
		public int			PreviousIndex	{ get; private set; }
		public int			Index			{ get; set; }
	}

	public sealed class NodeDropDownItem : NodeItem
	{
		public event EventHandler<AcceptNodeSelectionChangedEventArgs> SelectionChanged;

		public NodeDropDownItem(string[] items, int selectedIndex, bool inputEnabled, bool outputEnabled) :
			base(inputEnabled, outputEnabled)
		{
			this.Items = items.ToArray();
			this.SelectedIndex = selectedIndex;
		}

		#region Name
		public string Name
		{
			get;
			set;
		}
		#endregion

		#region SelectedIndex
		private int internalSelectedIndex = -1;
		public int SelectedIndex
		{
			get { return internalSelectedIndex; }
			set
			{
				if (internalSelectedIndex == value)
					return;
				if (SelectionChanged != null)
				{
					var eventArgs = new AcceptNodeSelectionChangedEventArgs(internalSelectedIndex, value);
					SelectionChanged(this, eventArgs);
					if (eventArgs.Cancel)
						return;
					internalSelectedIndex = eventArgs.Index;
				} else
					internalSelectedIndex = value;
				TextSize = Size.Empty;
			}
		}
		#endregion

		#region Items
		public string[] Items
		{
			get;
			set;
		}
		#endregion

		internal SizeF TextSize;

		public override bool OnDoubleClick()
		{
			base.OnDoubleClick();
			var form = new SelectionForm();
			form.Text = Name ?? "Select item from list";
			form.Items = Items;
			form.SelectedIndex = SelectedIndex;
			var result = form.ShowDialog();
			if (result == DialogResult.OK)
				SelectedIndex = form.SelectedIndex;
			return true;
		}

        public override SizeF Measure(Graphics graphics)
		{
			var text = string.Empty;
			if (Items != null &&
				SelectedIndex >= 0 && SelectedIndex < Items.Length)
				text = Items[SelectedIndex];
			if (!string.IsNullOrWhiteSpace(text))
			{
				if (this.TextSize.IsEmpty)
				{
					var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);

					this.TextSize = graphics.MeasureString(text, SystemFonts.MenuFont, size, GraphConstants.LeftMeasureTextStringFormat);
					
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
			var text = string.Empty;
			if (Items != null &&
				SelectedIndex >= 0 && SelectedIndex < Items.Length)
				text = Items[SelectedIndex];

			var size = Measure(graphics);
			size.Width  = Math.Max(minimumSize.Width, size.Width);
			size.Height = Math.Max(minimumSize.Height, size.Height);

			var path = GraphRenderer.CreateRoundedRectangle(size, location);

			location.Y += 1;
			location.X += 1;

			if ((state & RenderState.Hover) == RenderState.Hover)
			{
				graphics.DrawPath(Pens.White, path);
				graphics.DrawString(text, SystemFonts.MenuFont, Brushes.Black, new RectangleF(location, size), GraphConstants.LeftTextStringFormat);
			} else
			{
				graphics.DrawPath(Pens.Black, path);
				graphics.DrawString(text, SystemFonts.MenuFont, Brushes.Black, new RectangleF(location, size), GraphConstants.LeftTextStringFormat);
			}
		}

        public override void RenderConnector(Graphics graphics, RectangleF rectangle) { }
	}
}
