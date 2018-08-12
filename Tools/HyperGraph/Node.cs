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
using System.Drawing;
using System.ComponentModel;
using HyperGraph.Items;

namespace HyperGraph
{
	public class Node : IElement
	{
		public string			Title			{ get { return titleItem.Title; } set { titleItem.Title = value; } }

		#region Collapsed
		internal bool			internalCollapsed;
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
				if (Collapsed != oldValue)
					titleItem.ForceResize();
			} 
		}
		#endregion

		public bool				HasNoItems		{ get { return nodeItems.Count == 0; } }

		public PointF			Location		{ get; set; }
		public object			Tag				{ get; set; }

        public object           SubGraphTag     { get; set; }

		public IEnumerable<NodeConnection>	Connections { get { return connections; } }
		public IEnumerable<NodeItem>		Items		{ get { return nodeItems; } }

        public IEnumerable<NodeConnector>   InputConnectors
        {
            get
            {
                foreach (var i in nodeItems)
                    if (i.Input != null && i.Input.Enabled)
                        yield return i.Input;
            }
        }

        public IEnumerable<NodeConnector> OutputConnectors
        {
            get
            {
                foreach (var i in nodeItems)
                    if (i.Output != null && i.Output.Enabled)
                        yield return i.Output;
            }
        }

        public RectangleF		bounds;
		internal RenderState	state			= RenderState.None;
		internal RenderState	inputState		= RenderState.None;
		internal RenderState	outputState		= RenderState.None;

		private readonly List<NodeConnection>	connections			= new List<NodeConnection>();
		internal readonly NodeTitleItem			titleItem			= new NodeTitleItem();
		private readonly List<NodeItem>			nodeItems			= new List<NodeItem>();

		public Node(string title)
		{
			this.Title = title;
			titleItem.Node = this;
		}

		public void AddItem(NodeItem item)
		{
			if (nodeItems.Contains(item))
				return;
			if (item.Node != null)
				item.Node.RemoveItem(item);
			nodeItems.Add(item);
			item.Node = this;
		}

		public void RemoveItem(NodeItem item)
		{
			if (!nodeItems.Contains(item))
				return;
			item.Node = null;
			nodeItems.Remove(item);
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

        // Returns true if there are some connections that aren't connected
        public bool AnyConnectorsDisconnected
		{
			get
			{
				foreach (var item in nodeItems)
				{
					if (item.Input.Enabled && !item.Input.HasConnection)
						return true;
					if (item.Output.Enabled && !item.Output.HasConnection)
						return true;
				}
				return false;
			}
		}

		// Returns true if there are some output connections that aren't connected
		public bool AnyOutputConnectorsDisconnected
		{
			get
			{
				foreach (var item in nodeItems)
					if (item.Output.Enabled && !item.Output.HasConnection)
						return true;
				return false;
			}
		}

		// Returns true if there are some input connections that aren't connected
		public bool AnyInputConnectorsDisconnected
		{
			get
			{
				foreach (var item in nodeItems)
					if (item.Input.Enabled && !item.Input.HasConnection)
						return true;
				return false;
			}
		}

		public ElementType ElementType { get { return ElementType.Node; } }
	}
}
