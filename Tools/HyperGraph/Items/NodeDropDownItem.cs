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

        private static Brush BackgroundBrush = new SolidBrush(Color.FromArgb(96, 96, 96));

        public override bool OnClick(System.Windows.Forms.Control container, System.Windows.Forms.MouseEventArgs evnt, System.Drawing.Drawing2D.Matrix viewTransform)
		{
			base.OnClick(container, evnt, viewTransform);

            if (evnt.Button == System.Windows.Forms.MouseButtons.Left)
            {
                var basePts = new PointF[] { 
                    new PointF(Node.itemsBounds.Left + GraphConstants.HorizontalSpacing, bounds.Top), 
                    new PointF(Node.itemsBounds.Right + GraphConstants.HorizontalSpacing - GraphConstants.NodeExtraWidth, bounds.Bottom) };
                viewTransform.TransformPoints(basePts);

                var dropDownCtrl = new ListBox();
                dropDownCtrl.BorderStyle = BorderStyle.None;
                dropDownCtrl.Margin = new Padding(0);
                dropDownCtrl.Padding = new Padding(0);

                dropDownCtrl.DrawMode = DrawMode.OwnerDrawVariable;
                dropDownCtrl.DrawItem +=
                    (object sender, DrawItemEventArgs e) =>
                    {
                        var lb = sender as ListBox;
                        var item = lb.Items[e.Index];

                        bool selectedState = e.State != DrawItemState.None;

                        // We have to draw the background every item, because the control 
                        // background isn't refreshed on state changes
                        e.Graphics.FillRectangle(selectedState ? Brushes.Gray : BackgroundBrush, e.Bounds);
                        e.Graphics.DrawString(
                            item.ToString(), SystemFonts.MenuFont, selectedState ? Brushes.Black : Brushes.White,
                            e.Bounds, GraphConstants.LeftTextStringFormat);
                    };

                dropDownCtrl.MeasureItem +=
                    (object sender, MeasureItemEventArgs e) =>
                    {
                        var lb = sender as ListBox;
                        var item = lb.Items[e.Index];
                        var size = new Size(GraphConstants.MinimumItemWidth, GraphConstants.MinimumItemHeight);
                        var textSize = e.Graphics.MeasureString(
                            item.ToString(), SystemFonts.MenuFont,
                            size, GraphConstants.LeftMeasureTextStringFormat);
                        e.ItemWidth = (int)textSize.Width;
                        e.ItemHeight = (int)textSize.Height;
                    };

                dropDownCtrl.BackColor = Color.FromArgb(96, 96, 96);

                dropDownCtrl.Items.AddRange(Items);
                dropDownCtrl.SelectedIndex = SelectedIndex;
                
                var toolDrop = new ToolStripDropDown();
                var toolHost = new ToolStripControlHost(dropDownCtrl);
                toolHost.Margin = new Padding(0);
                toolDrop.Padding = new Padding(0);
                toolDrop.Items.Add(toolHost);

                // Unfortunately the AutoSize functionality for toolHost just doesn't
                // work with an owner draw list box... Perhaps MeasureItem isn't called
                // until the list box is first drawn -- but that is after the tool host
                // has done it's auto size
                toolHost.AutoSize = false;
                toolHost.Size = new Size((int)(basePts[1].X - basePts[0].X), dropDownCtrl.PreferredHeight + 20);
                
                dropDownCtrl.SelectedIndexChanged +=
                    (object sender, System.EventArgs e) =>
                    {
                        var lb = sender as ListBox;
                        if (lb != null)
                            SelectedIndex = lb.SelectedIndex;
                        toolDrop.Close();
                    };

                toolDrop.Show(container, new Point((int)basePts[0].X, (int)basePts[1].Y + 4));
                return true;
            }

            return false;
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

        public override void Render(Graphics graphics, SizeF minimumSize, PointF location, object context)
		{
			var text = string.Empty;
			if (Items != null &&
				SelectedIndex >= 0 && SelectedIndex < Items.Length)
				text = Items[SelectedIndex];

			var size = Measure(graphics);
			size.Width  = Math.Max(minimumSize.Width, size.Width);
			size.Height = Math.Max(minimumSize.Height, size.Height);

			var path = GraphRenderer.CreateRoundedRectangle(size, location);

            var stringRect = new RectangleF(new PointF(location.X + 1, location.Y + 1), new SizeF(size.Width - 2, size.Height - 2));
            var arrowRect = stringRect;

            float sep = 2.0f;
            float arrowWidth = Math.Min(arrowRect.Height, stringRect.Width - sep);
            arrowRect.X += arrowRect.Width - arrowWidth;
            arrowRect.Width = arrowWidth;
            stringRect.Width -= arrowWidth - sep;
            arrowRect.X += 6; arrowRect.Width -= 12;
            arrowRect.Y += 6; arrowRect.Height -= 12;

            bool highlight = (state & RenderState.Hover) == RenderState.Hover;

            graphics.FillPath(BackgroundBrush, path);
            graphics.DrawPath(highlight ? Pens.White : Pens.LightGray, path);
            graphics.DrawString(text, SystemFonts.MenuFont, Brushes.White, stringRect, GraphConstants.CenterTextStringFormat);

                // draw a little arrow to indicate that it is a drop down list
            graphics.FillPolygon(
                highlight ? Brushes.White : Brushes.LightGray,
                new Point[] 
                    {
                        new Point((int)arrowRect.Left, (int)arrowRect.Top),
                        new Point((int)arrowRect.Right, (int)arrowRect.Top),
                        new Point((int)(.5f * (arrowRect.Left + arrowRect.Right)), (int)arrowRect.Bottom)
                    });

		}

        public override void RenderConnector(Graphics graphics, RectangleF rectangle) { }
	}
}
