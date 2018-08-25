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

using System.Collections.Generic;
using System.Drawing;
using HyperGraph.Items;

namespace HyperGraph
{
	public class Node : IElement
	{
        #region Collapsed
        private bool			internalCollapsed;
		public bool				Collapsed		
		{ 
			get 
			{
                        // disabling "collapse" on DraggedOver gives an interesting
                        // result... but also gets in the way a lot of the time
                // return (internalCollapsed && 
                // 		((state & RenderState.DraggedOver) == 0)) ||
                // 		nodeItems.Count == 0;
                return internalCollapsed;
			} 
			set 
			{
				var oldValue = Collapsed;
				internalCollapsed = value;
			} 
		}
		#endregion

		public bool				HasNoItems		{ get { return (inputItems.Count == 0) && (centerItems.Count == 0) && (outputItems.Count == 0); } }

		public PointF			Location		{ get; set; }
		public object			Tag				{ get; set; }

        public object           SubGraphTag     { get; set; }

		public IEnumerable<NodeConnection>	Connections { get { return connections; } }

		public IEnumerable<NodeItem>		InputItems		{ get { return inputItems; } }
        public IEnumerable<NodeItem>        TopItems        { get { return topItems; } }
        public IEnumerable<NodeItem>        CenterItems     { get { return centerItems; } }
        public IEnumerable<NodeItem>        BottomItems     { get { return bottomItems; } }
        public IEnumerable<NodeItem>        OutputItems     { get { return outputItems; } }

        public enum Dock {  Input, Top, Center, Bottom, Output };
        public IEnumerable<NodeItem>        ItemsForDock(Dock c)
        {
            if (c == Dock.Input) return InputItems;
            if (c == Dock.Top) return TopItems;
            if (c == Dock.Center) return CenterItems;
            if (c == Dock.Bottom) return BottomItems;
            if (c == Dock.Output) return OutputItems;
            return null;
        }

        public IEnumerable<NodeConnector>   InputConnectors
        {
            get
            {
                foreach (var i in InputItems)
                {
                    var c = i as NodeConnector;
                    if (c != null)
                        yield return c;
                }
            }
        }

        public IEnumerable<NodeConnector> OutputConnectors
        {
            get
            {
                foreach (var i in OutputItems)
                {
                    var c = i as NodeConnector;
                    if (c != null)
                        yield return c;
                }
            }
        }

        private NodeItem titleItem_ = null;
        public NodeItem TitleItem
        {
            get { return titleItem_; }
            set
            {
                if (titleItem_ != null)
                    RemoveItem(titleItem_);
                if (value.Node != null)
                    value.Node.RemoveItem(value);
                centerItems.Insert(0, value);
                value.Node = this;
                titleItem_ = value;
            }
        }

        public string Title {
            get {
                var titleItem = TitleItem as NodeTitleItem;
                return (titleItem != null) ? titleItem.Title : string.Empty;
            }
            set {
                TitleItem = new NodeTitleItem { Title = value };
            }
        }

        public RectangleF		bounds;
		internal RenderState	state			= RenderState.None;
		internal RenderState	inputState		= RenderState.None;
		internal RenderState	outputState		= RenderState.None;

		private readonly List<NodeConnection>	connections			= new List<NodeConnection>();
		private readonly List<NodeItem>			inputItems			= new List<NodeItem>();
        private readonly List<NodeItem>         topItems            = new List<NodeItem>();
        private readonly List<NodeItem>         centerItems         = new List<NodeItem>();
        private readonly List<NodeItem>         bottomItems         = new List<NodeItem>();
        private readonly List<NodeItem>         outputItems         = new List<NodeItem>();

		public void AddItem(NodeItem item, Dock column)
		{
			if (item.Node != null)
				item.Node.RemoveItem(item);
            switch (column)
            {
            case Dock.Input: inputItems.Add(item); break;
            case Dock.Top: topItems.Add(item); break;
            case Dock.Center: centerItems.Add(item); break;
            case Dock.Bottom: bottomItems.Add(item); break;
            case Dock.Output: outputItems.Add(item); break;
            }
			item.Node = this;
		}

		public void RemoveItem(NodeItem item)
		{
			item.Node = null;
			inputItems.Remove(item);
            topItems.Remove(item);
            centerItems.Remove(item);
            bottomItems.Remove(item);
            outputItems.Remove(item);
        }

        public void AddConnection(NodeConnection newConnection)
        {
            connections.Add(newConnection);
        }

        public void RemoveConnection(NodeConnection connection)
        {
            connections.Remove(connection);
        }

        public bool MoveToFront(NodeConnection connection)
        {
            if (connections[0] == connection) return false;
            connections.Remove(connection);
            connections.Insert(0, connection);
            return true;
        }

		public ElementType ElementType { get { return ElementType.Node; } }
	}
}
