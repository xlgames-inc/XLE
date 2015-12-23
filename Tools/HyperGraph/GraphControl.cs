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
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using HyperGraph.Compatibility;

namespace HyperGraph
{
	public delegate bool AcceptElement(IElement element);

	public partial class GraphControl : Control
	{
		#region Constructor
		public GraphControl()
		{
			InitializeComponent();
			this.SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.Opaque | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.Selectable | ControlStyles.UserPaint, true);
			CompatibilityStrategy = new AlwaysCompatible();
		}
		#endregion

		public event EventHandler<ElementEventArgs>					FocusChanged;
		public event EventHandler<AcceptNodeEventArgs>				NodeAdded;
		public event EventHandler<AcceptNodeEventArgs>				NodeRemoving;
		public event EventHandler<NodeEventArgs>					NodeRemoved;
		public event EventHandler<AcceptElementLocationEventArgs>	ShowElementMenu;
		public event EventHandler<AcceptNodeConnectionEventArgs>	ConnectionAdding;
		public event EventHandler<AcceptNodeConnectionEventArgs>	ConnectionAdded;
		public event EventHandler<AcceptNodeConnectionEventArgs>	ConnectionRemoving;
		public event EventHandler<NodeConnectionEventArgs>			ConnectionRemoved;

		#region Grid
		public bool		ShowGrid					= true;
		public float	internalSmallGridStep		= 16.0f;
		[Description("The distance between the smallest grid lines"), Category("Appearance")] 
		public float	SmallGridStep
		{ 
			get 
			{
				return internalSmallGridStep; 
			}
			set
			{
				if (internalSmallGridStep == value)
					return;

				internalSmallGridStep = value;
				this.Invalidate();
			}
		}
		public float	internalLargeGridStep			= 16.0f * 8.0f;
		[Description("The distance between the largest grid lines"), Category("Appearance")] 
		public float	LargeGridStep
		{ 
			get 
			{ 
				return internalLargeGridStep; 
			}
			set
			{
				if (internalLargeGridStep == value)
					return;

				internalLargeGridStep = value;
				this.Invalidate();
			}
		}
		private Color	internalSmallStepGridColor	= Color.Gray;
		private Pen		SmallGridPen				= new Pen(Color.Gray);
		[Description("The color for the grid lines with the smallest gap between them"), Category("Appearance")] 
		public Color	SmallStepGridColor
		{
			get { return internalSmallStepGridColor; }
			set
			{
				if (internalSmallStepGridColor == value)
					return;

				internalSmallStepGridColor = value;
				SmallGridPen = new Pen(internalSmallStepGridColor);
				this.Invalidate();
			}
		}
		private Color	internalLargeStepGridColor	= Color.LightGray;
		private Pen		LargeGridPen				= new Pen(Color.LightGray);
		[Description("The color for the grid lines with the largest gap between them"), Category("Appearance")] 
		public Color	LargeStepGridColor
		{
			get { return internalLargeStepGridColor; }
			set
			{
				if (internalLargeStepGridColor == value)
					return;

				internalLargeStepGridColor = value;
				LargeGridPen = new Pen(internalLargeStepGridColor);
				this.Invalidate();
			}
		}
		#endregion

		#region DragElement
		IElement internalDragElement;
		IElement DragElement
		{
			get { return internalDragElement; }
			set
			{
				if (internalDragElement == value)
					return;
				if (internalDragElement != null)
					SetFlag(internalDragElement, RenderState.Dragging, false, false);
				internalDragElement = value;
				if (internalDragElement != null)
					SetFlag(internalDragElement, RenderState.Dragging, true, false);
			}
		}
		#endregion
		
		#region HoverElement
		IElement internalHoverElement;
		IElement HoverElement
		{
			get { return internalHoverElement; }
			set
			{
				if (internalHoverElement == value)
					return;
				if (internalHoverElement != null)
					SetFlag(internalHoverElement, RenderState.Hover, false, true);
				internalHoverElement = value;
				if (internalHoverElement != null)
					SetFlag(internalHoverElement, RenderState.Hover, true, true);
			}
		}
		#endregion
		
		#region FocusElement
		IElement internalFocusElement;
		[Browsable(false), EditorBrowsable(EditorBrowsableState.Never)]
		public IElement FocusElement
		{
			get { return internalFocusElement; }
			set
			{
				if (internalFocusElement == value)
					return;
				if (internalFocusElement != null)
					SetFlag(internalFocusElement, RenderState.Focus, false, false);
				internalFocusElement = value;
				if (internalFocusElement != null)
					SetFlag(internalFocusElement, RenderState.Focus, true, false);

				if (FocusChanged != null)
					FocusChanged(this, new ElementEventArgs(value));

				this.Invalidate();
			}
		}
		#endregion


		#region SetFlag
		RenderState SetFlag(RenderState original, RenderState flag, bool value)
		{
			if (value)
				return original | flag;
			else
				return original & ~flag;
		}
		void SetFlag(IElement element, RenderState flag, bool value)
		{
			if (element == null)
				return;

			switch (element.ElementType)
			{
				case ElementType.NodeSelection:
				{
					var selection = element as NodeSelection;
					foreach (var node in selection.Nodes)
					{
						node.state = SetFlag(node.state, flag, value);
						SetFlag(node.titleItem, flag, value);
					}
					break;
				}

				case ElementType.Node:
				{
					var node = element as Node;
					node.state = SetFlag(node.state, flag, value);
					SetFlag(node.titleItem, flag, value);
					break;
				}

				case ElementType.InputConnector:
				case ElementType.OutputConnector:
					var connector = element as NodeConnector;
					connector.state = SetFlag(connector.state, flag, value);
					break;

				case ElementType.Connection:
					var connection = element as NodeConnection;
					connection.state = SetFlag(connection.state, flag, value);
					break;

				case ElementType.NodeItem:
					var item = element as NodeItem;
					item.state = SetFlag(item.state, flag, value);
					break;
			}
		}
		void SetFlag(IElement element, RenderState flag, bool value, bool setConnections)
		{
			if (element == null)
				return;

			switch (element.ElementType)
			{
				case ElementType.NodeSelection:
				{
					var selection = element as NodeSelection;
					foreach (var node in selection.Nodes)
					{
						node.state = SetFlag(node.state, flag, value);
						SetFlag(node.titleItem, flag, value);
					}
					break;
				}

				case ElementType.Node:
				{
					var node = element as Node;
					node.state = SetFlag(node.state, flag, value);
					SetFlag(node.titleItem, flag, value);
					break;
				}

				case ElementType.InputConnector:
				case ElementType.OutputConnector:
					var connector = element as NodeConnector;
					connector.state = SetFlag(connector.state, flag, value);
					SetFlag(connector.Node, flag, value, setConnections);
					break;

				case ElementType.Connection:
					var connection = element as NodeConnection;
					connection.state = SetFlag(connection.state, flag, value);
					if (setConnections)
					{
						//if (connection.From != null)
						//	connection.From.state = SetFlag(connection.From.state, flag, value);
						//if (connection.To != null)
						//	connection.To.state = SetFlag(connection.To.state, flag, value);
						//SetFlag(connection.From, flag, value, setConnections);
						//SetFlag(connection.To, flag, value, setConnections);
					}
					break;

				case ElementType.NodeItem:
					var item = element as NodeItem;
					item.state = SetFlag(item.state, flag, value);
					SetFlag(item.Node, flag, value, setConnections);
					break;
			}
		}
		#endregion

		#region BringElementToFront
		public void BringElementToFront(IElement element)
		{
			if (element == null)
				return;
			switch (element.ElementType)
			{
				case ElementType.Connection:
					var connection = element as NodeConnection;
					BringElementToFront(connection.From);
					BringElementToFront(connection.To);
					
					var connections = connection.From.Node.connections;
					if (connections[0] != connection)
					{
						connections.Remove(connection);
						connections.Insert(0, connection);
					}
					
					connections = connection.To.Node.connections;
					if (connections[0] != connection)
					{
						connections.Remove(connection);
						connections.Insert(0, connection);
					}
					break;
				case ElementType.NodeSelection:
				{
					var selection = element as NodeSelection;
					foreach(var node in selection.Nodes.Reverse<Node>())
					{
						if (graphNodes[0] != node)
						{
							graphNodes.Remove(node);
							graphNodes.Insert(0, node);
						}
					}
					break;
				}
				case ElementType.Node:
				{
					var node = element as Node;
					if (graphNodes[0] != node)
					{
						graphNodes.Remove(node);
						graphNodes.Insert(0, node);
					}
					break;
				}
				case ElementType.InputConnector:
				case ElementType.OutputConnector:
					var connector = element as NodeConnector;
					BringElementToFront(connector.Node);
					break;
				case ElementType.NodeItem:
					var item = element as NodeItem;
					BringElementToFront(item.Node);
					break;
			}
		}
		#endregion
		
		#region HasFocus
		bool HasFocus(IElement element)
		{
			if (element == null)
				return FocusElement == null;

			if (FocusElement == null)
				return false;

			if (element.ElementType ==
				FocusElement.ElementType)
				return (element == FocusElement);
			
			switch (FocusElement.ElementType)
			{
				case ElementType.Connection:
					var focusConnection = FocusElement as NodeConnection;
					return (focusConnection.To == element ||
							focusConnection.From == element ||
							
							((focusConnection.To != null &&
							focusConnection.To.Node == element) ||
							(focusConnection.From != null &&
							focusConnection.From.Node == element)));
				case ElementType.NodeItem:
					var focusItem = FocusElement as NodeItem;
					return (focusItem.Node == element);
				case ElementType.InputConnector:
				case ElementType.OutputConnector:
					var focusConnector = FocusElement as NodeConnector;
					return (focusConnector.Node == element);
				case ElementType.NodeSelection:
				{
					var selection = FocusElement as NodeSelection;
					foreach (var node in selection.Nodes)
					{
						if (node == element)
							return true;
					}
					return false;
				}
				default:
				case ElementType.Node:
					return false;
			}
		}
		#endregion


		#region ShowLabels
		bool internalShowLabels = false;
		[Description("Show labels on the lines that connect the graph nodes"), Category("Appearance")]
		public bool ShowLabels 
		{ 
			get 
			{
				return internalShowLabels;
			} 
			set
			{
				if (internalShowLabels == value)
					return;
				internalShowLabels = value;
				this.Invalidate();
			}
		}
		#endregion

		#region HighlightCompatible
		/// <summary>
		/// Should compatible connectors be highlighted when dragging a connection?
		/// </summary>
		[DisplayName( "Highlight Compatible Node Items" )]
		[Description( "Should compatible connectors be highlighted when dragging a connection?" )]
		[Category( "Behavior" )]
		public bool HighlightCompatible { get; set; }

		/// <summary>
		/// The strategy that will be applied to determine if two node item connectors are compatible with each other
		/// </summary>
		[Browsable(false), EditorBrowsable(EditorBrowsableState.Never)]
		public ICompatibilityStrategy CompatibilityStrategy { get; set; }
		#endregion


		#region Nodes
		readonly List<Node> graphNodes = new List<Node>();
		[Browsable(false), EditorBrowsable(EditorBrowsableState.Never)]
		public IEnumerable<Node> Nodes { get { return graphNodes; } }
		#endregion


		enum CommandMode
		{
			MarqueSelection,
			TranslateView,
			ScaleView,
			Edit
		}


		IElement				internalDragOverElement;
		bool					mouseMoved		= false;
		bool					dragging		= false;
		bool					abortDrag		= false;
		readonly List<Node>		selectedNodes	= new List<Node>();
		readonly List<Node>		unselectedNodes	= new List<Node>();
		CommandMode				command			= CommandMode.Edit;
		MouseButtons			currentButtons;

		Point					lastLocation;
		PointF					snappedLocation;
		PointF					originalLocation;
		Point					originalMouseLocation;
		
		PointF					translation = new PointF();
		float					zoom = 1.0f;

		
		#region UpdateMatrices
		readonly Matrix			transformation = new Matrix();
		readonly Matrix			inverse_transformation = new Matrix();
		void UpdateMatrices()
		{
			if (zoom < 0.25f) zoom = 0.25f;
			if (zoom > 5.00f) zoom = 5.00f;
			var center = new PointF(this.Width / 2.0f, this.Height / 2.0f);
			transformation.Reset();
			transformation.Translate(translation.X, translation.Y);
			transformation.Translate(center.X, center.Y);
			transformation.Scale(zoom, zoom);
			transformation.Translate(-center.X, -center.Y);

			inverse_transformation.Reset();
			inverse_transformation.Translate(center.X, center.Y);
			inverse_transformation.Scale(1.0f / zoom, 1.0f / zoom);
			inverse_transformation.Translate(-center.X, -center.Y);
			inverse_transformation.Translate(-translation.X, -translation.Y);
		}
		#endregion



		#region AddNode
		public bool AddNode(Node node)
		{
			if (node == null ||
				graphNodes.Contains(node))
				return false;

			graphNodes.Insert(0, node);			
			if (NodeAdded != null)
			{
				var eventArgs = new AcceptNodeEventArgs(node);
				NodeAdded(this, eventArgs);
				if (eventArgs.Cancel)
				{
					graphNodes.Remove(node);
					return false;
				}
			}

			BringElementToFront(node);
			FocusElement = node;
			this.Invalidate();
			return true;
		}
		#endregion

		#region AddNodes
		public bool AddNodes(IEnumerable<Node> nodes)
		{
			if (nodes == null)
				return false;

			int		index		= 0;
			bool	modified	= false;
			Node	lastNode	= null;
			foreach (var node in nodes)
			{
				if (node == null)
					continue;
				if (graphNodes.Contains(node))
					continue;

				graphNodes.Insert(index, node); index++;

				if (NodeAdded != null)
				{
					var eventArgs = new AcceptNodeEventArgs(node);
					NodeAdded(this, eventArgs);
					if (eventArgs.Cancel)
					{
						graphNodes.Remove(node);
						modified = true;
					} else
						lastNode = node;
				} else
					lastNode = node;
			}
			if (lastNode != null)
			{
				BringElementToFront(lastNode);
				FocusElement = lastNode;
				this.Invalidate();
			}
			return modified;
		}
		#endregion

		#region RemoveNode
		public void RemoveNode(Node node)
		{
			if (node == null)
				return;

			if (NodeRemoving != null)
			{
				var eventArgs = new AcceptNodeEventArgs(node);
				NodeRemoving(this, eventArgs);
				if (eventArgs.Cancel)
					return;
			}
			if (HasFocus(node))
				FocusElement = null;

			DisconnectAll(node);
			graphNodes.Remove(node);
			this.Invalidate();

			if (NodeRemoved != null)
				NodeRemoved(this, new NodeEventArgs(node));
		}
		#endregion

		#region RemoveNodes
		public bool RemoveNodes(IEnumerable<Node> nodes)
		{
			if (nodes == null)
				return false;

			bool modified = false;
			foreach (var node in nodes)
			{
				if (node == null)
					continue;
				if (NodeRemoving != null)
				{
					var eventArgs = new AcceptNodeEventArgs(node);
					NodeRemoving(this, eventArgs);
					if (eventArgs.Cancel)
						continue;
				}

				if (HasFocus(node))
					FocusElement = null;

				DisconnectAll(node);
				graphNodes.Remove(node);
				modified = true;

				if (NodeRemoved != null)
					NodeRemoved(this, new NodeEventArgs(node));
			}
			if (modified)
				this.Invalidate();
			return modified;
		}
		#endregion

		#region Connect
		public NodeConnection Connect(NodeItem from, NodeItem to)
		{
			return Connect(from.Output, to.Input);
		}

		public NodeConnection Connect(NodeConnector from, NodeConnector to)
		{
			if (from      == null || to      == null ||
				from.Node == null || to.Node == null ||
				!from.Enabled || 
				!to.Enabled)
				return null;

			foreach (var other in from.Node.connections)
			{
				if (other.From == from &&
					other.To == to)
					return null;
			}

			foreach (var other in to.Node.connections)
			{
				if (other.From == from &&
					other.To == to)
					return null;
			}

			var connection = new NodeConnection();
			connection.From = from;
			connection.To = to;

			from.Node.connections.Add(connection);
			to.Node.connections.Add(connection);
			
			if (ConnectionAdded != null)
			{
				var eventArgs = new AcceptNodeConnectionEventArgs(connection);
				ConnectionAdded(this, eventArgs);
				if (eventArgs.Cancel)
				{
					Disconnect(connection);
					return null;
				}
			}

			return connection;
		}
		#endregion

		#region Disconnect
		public bool Disconnect(NodeConnection connection)
		{
			if (connection == null)
				return false;

			if (ConnectionRemoving != null)
			{
				var eventArgs = new AcceptNodeConnectionEventArgs(connection);
				ConnectionRemoving(this, eventArgs);
				if (eventArgs.Cancel)
					return false;
			}

			if (HasFocus(connection))
				FocusElement = null;
			
			var from	= connection.From;
			var to		= connection.To;
			if (from != null && from.Node != null)
			{
				from.Node.connections.Remove(connection);
			}
			if (to != null && to.Node != null)
			{
				to.Node.connections.Remove(connection);
			}

			// Just in case somebody stored it somewhere ..
			connection.From = null;
			connection.To = null;

			if (ConnectionRemoved != null)
				ConnectionRemoved(this, new NodeConnectionEventArgs(from, to, connection));

			this.Invalidate();
			return true;
		}
		#endregion

		#region DisconnectAll (private)
		bool DisconnectAll(Node node)
		{
			bool modified = false;
			var connections = node.connections.ToList();
			foreach (var connection in connections)
				modified = Disconnect(connection) ||
					modified;
			return modified;
		}
		#endregion


		#region FindNodeItemAt
		static NodeItem FindNodeItemAt(Node node, PointF location)
		{
			if (node.itemsBounds == null ||
				location.X < node.itemsBounds.Left ||
				location.X > node.itemsBounds.Right)
				return null;

			foreach (var item in node.Items)
			{
				if (item.bounds.IsEmpty)
					continue;

				if (location.Y < item.bounds.Top)
					break;

				if (location.Y < item.bounds.Bottom)
					return item;
			}
			return null;
		}
		#endregion

		#region FindInputConnectorAt
		static NodeConnector FindInputConnectorAt(Node node, PointF location)
		{
			if (node.itemsBounds == null || node.Collapsed)
				return null;

			foreach (var inputConnector in node.inputConnectors)
			{
				if (inputConnector.bounds.IsEmpty)
					continue;

				if (inputConnector.bounds.Contains(location))
					return inputConnector;
			}
			return null;
		}
		#endregion

		#region FindOutputConnectorAt
		static NodeConnector FindOutputConnectorAt(Node node, PointF location)
		{
			if (node.itemsBounds == null || node.Collapsed)
				return null;

			foreach (var outputConnector in node.outputConnectors)
			{
				if (outputConnector.bounds.IsEmpty)
					continue;

				if (outputConnector.bounds.Contains(location))
					return outputConnector;
			}
			return null;
		}
		#endregion

		#region FindElementAt
		IElement FindElementAt(PointF location)
		{
			foreach (var node in graphNodes)
			{
				var inputConnector = FindInputConnectorAt(node, location);
				if (inputConnector != null)
					return inputConnector;

				var outputConnector = FindOutputConnectorAt(node, location);
				if (outputConnector != null)
					return outputConnector;

				if (node.bounds.Contains(location))
				{
					var item = FindNodeItemAt(node, location);
					if (item != null)
						return item;
					return node;
				}
			}

			var skipConnections		= new HashSet<NodeConnection>();
			var foundConnections	= new List<NodeConnection>();
			foreach (var node in graphNodes)
			{
				foreach (var connection in node.connections)
				{
					if (skipConnections.Add(connection)) // if we can add it, we haven't checked it yet
					{
						if (connection.bounds.Contains(location))
							foundConnections.Insert(0, connection);
					}
				}
			}
			foreach (var connection in foundConnections)
			{
				if (connection.textBounds.Contains(location))
					return connection;
			}
			foreach (var connection in foundConnections)
			{
				using (var region = GraphRenderer.GetConnectionRegion(connection))
				{
					if (region.IsVisible(location))
						return connection;
				}
			}

			return null;
		}
		#endregion
		
		#region FindElementAt
		IElement FindElementAt(PointF location, AcceptElement acceptElement)
		{
			foreach (var node in graphNodes)
			{
				var inputConnector = FindInputConnectorAt(node, location);
				if (inputConnector != null && acceptElement(inputConnector))
					return inputConnector;

				var outputConnector = FindOutputConnectorAt(node, location);
				if (outputConnector != null && acceptElement(outputConnector))
					return outputConnector;

				if (node.bounds.Contains(location))
				{
					var item = FindNodeItemAt(node, location);
					if (item != null && acceptElement(item))
						return item;
					if (acceptElement(node))
						return node;
					else
						return null;
				}
			}

			var skipConnections		= new HashSet<NodeConnection>();
			var foundConnections	= new List<NodeConnection>();
			foreach (var node in graphNodes)
			{
				foreach (var connection in node.connections)
				{
					if (skipConnections.Add(connection)) // if we can add it, we haven't checked it yet
					{
						if (connection.bounds.Contains(location))
							foundConnections.Insert(0, connection);
					}
				}
			}
			foreach (var connection in foundConnections)
			{
				if (connection.textBounds.Contains(location) && acceptElement(connection))
					return connection;
			}
			foreach (var connection in foundConnections)
			{
				using (var region = GraphRenderer.GetConnectionRegion(connection))
				{
					if (region.IsVisible(location) && acceptElement(connection))
						return connection;
				}
			}

			return null;
		}
		#endregion
		
		#region GetTransformedLocation
		PointF GetTransformedLocation()
		{
			var points = new PointF[] { snappedLocation };
			inverse_transformation.TransformPoints(points);
			var transformed_location = points[0];

			if (abortDrag)
			{
				transformed_location = originalLocation;
			}
			return transformed_location;
		}
		#endregion

		#region GetMarqueRectangle
		RectangleF GetMarqueRectangle()
		{
			var transformed_location = GetTransformedLocation();
			var x1 = transformed_location.X;
			var y1 = transformed_location.Y;
			var x2 = originalLocation.X;
			var y2 = originalLocation.Y;
			var x = Math.Min(x1, x2);
			var y = Math.Min(y1, y2);
			var width = Math.Max(x1, x2) - x;
			var height = Math.Max(y1, y2) - y;
			return new RectangleF(x,y,width,height);
		}				
		#endregion

		#region OnPaint
		protected override void OnPaint(PaintEventArgs e)
		{
			base.OnPaint(e);

			if (e.Graphics == null)
				return;
			

			e.Graphics.PageUnit				= GraphicsUnit.Pixel;
			e.Graphics.CompositingQuality	= CompositingQuality.GammaCorrected;
			e.Graphics.TextRenderingHint	= TextRenderingHint.ClearTypeGridFit;
			e.Graphics.PixelOffsetMode		= PixelOffsetMode.HighQuality;
			e.Graphics.InterpolationMode	= InterpolationMode.HighQualityBicubic;

			UpdateMatrices();			
			e.Graphics.Transform			= transformation;

			OnDrawBackground(e);
			
			e.Graphics.SmoothingMode		= SmoothingMode.HighQuality;

			if (this.graphNodes.Count == 0)
				return;

			
			var transformed_location = GetTransformedLocation();
			if (command == CommandMode.MarqueSelection)
			{
				var marque_rectangle = GetMarqueRectangle();
				e.Graphics.FillRectangle(SystemBrushes.ActiveCaption, marque_rectangle);
				e.Graphics.DrawRectangle(Pens.DarkGray, marque_rectangle.X, marque_rectangle.Y, marque_rectangle.Width, marque_rectangle.Height);
			}

			GraphRenderer.PerformLayout(e.Graphics, graphNodes);
			GraphRenderer.Render(e.Graphics, graphNodes, ShowLabels);
			
			if (command == CommandMode.Edit)
			{
				if (dragging)
				{
					if (DragElement != null)
					{
						RenderState renderState = RenderState.Dragging | RenderState.Hover;
						switch (DragElement.ElementType)
						{
							case ElementType.OutputConnector:
								var outputConnector = DragElement as NodeConnector;
                                renderState |= (outputConnector.state & (RenderState.Incompatible | RenderState.Compatible | RenderState.Conversion));
								GraphRenderer.RenderOutputConnection(e.Graphics, outputConnector, 
									transformed_location.X, transformed_location.Y, renderState);
								break;
							case ElementType.InputConnector:
								var inputConnector = DragElement as NodeConnector;
                                renderState |= (inputConnector.state & (RenderState.Incompatible | RenderState.Compatible | RenderState.Conversion));
								GraphRenderer.RenderInputConnection(e.Graphics, inputConnector, 
									transformed_location.X, transformed_location.Y, renderState);
								break;
						}
					}
				}
			}
		}
		#endregion

		#region OnDrawBackground
		virtual protected void OnDrawBackground(PaintEventArgs e)
		{
			e.Graphics.Clear(BackColor);

			if (!ShowGrid)
				return;

			var points		= new PointF[]{
								new PointF(e.ClipRectangle.Left , e.ClipRectangle.Top),
								new PointF(e.ClipRectangle.Right, e.ClipRectangle.Bottom)
							};

			inverse_transformation.TransformPoints(points);

			var left			= points[0].X;
			var right			= points[1].X;
			var top				= points[0].Y;
			var bottom			= points[1].Y;
			var smallStepScaled	= SmallGridStep;
			
			var smallXOffset	= ((float)Math.Round(left / smallStepScaled) * smallStepScaled);
			var smallYOffset	= ((float)Math.Round(top  / smallStepScaled) * smallStepScaled);

			if (smallStepScaled > 3)
			{
				for (float x = smallXOffset; x < right; x += smallStepScaled)
					e.Graphics.DrawLine(SmallGridPen, x, top, x, bottom);

				for (float y = smallYOffset; y < bottom; y += smallStepScaled)
					e.Graphics.DrawLine(SmallGridPen, left, y, right, y);
			}

			var largeStepScaled = LargeGridStep;
			var largeXOffset	= ((float)Math.Round(left / largeStepScaled) * largeStepScaled);
			var largeYOffset	= ((float)Math.Round(top  / largeStepScaled) * largeStepScaled);

			if (largeStepScaled > 3)
			{
				for (float x = largeXOffset; x < right; x += largeStepScaled)
					e.Graphics.DrawLine(LargeGridPen, x, top, x, bottom);

				for (float y = largeYOffset; y < bottom; y += largeStepScaled)
					e.Graphics.DrawLine(LargeGridPen, left, y, right, y);
			}
		}
		#endregion



		#region OnMouseWheel
		protected override void OnMouseWheel(MouseEventArgs e)
		{
			base.OnMouseWheel(e);

			zoom *= (float)Math.Pow(2, e.Delta / 480.0f);

			this.Refresh();
		}
		#endregion

		#region OnMouseDown
		protected override void OnMouseDown(MouseEventArgs e)
		{
			base.OnMouseDown(e);

			if (currentButtons != MouseButtons.None)
				return;

			currentButtons |= e.Button;
			selectedNodes.Clear();
			unselectedNodes.Clear();
			dragging	= true;
			abortDrag	= false;
			mouseMoved	= false;
			snappedLocation = lastLocation = e.Location;
			
			var points = new PointF[] { e.Location };
			inverse_transformation.TransformPoints(points);
			var transformed_location = points[0];

			originalLocation = transformed_location;

			if (e.Button == MouseButtons.Left)
			{
				var element = FindElementAt(transformed_location);
				if (element != null)
				{
					var selection = FocusElement as NodeSelection;				
					var element_node = element as Node;
					if (element_node != null)
					{
						switch (ModifierKeys)
						{
							case Keys.None:
							{
								if (selection != null &&
									selection.Nodes.Contains(element_node))
								{
									element = selection;
								}
								break;
							}
							case Keys.Shift:
							{
								if (selection != null)
								{
									if (!selection.Nodes.Contains(element_node))
									{
										var nodes = selection.Nodes.ToList();
										nodes.Add(element_node);
										element = new NodeSelection(nodes);
									}
								} else
								{
									var focus_node = FocusElement as Node;
									if (focus_node != null)
										element = new NodeSelection(new Node[] { focus_node, element_node });
								}
								break;
							}
							case Keys.Control:
							{
								if (selection != null)
								{
									if (selection.Nodes.Contains(element_node))
									{
										var nodes = selection.Nodes.ToList();
										nodes.Remove(element_node);
										element = new NodeSelection(nodes);
									} else
									{
										var nodes = selection.Nodes.ToList();
										nodes.Add(element_node);
										element = new NodeSelection(nodes);
									}
								} else
								{
									var focus_node = FocusElement as Node;
									if (focus_node != null)
									{
										if (focus_node == element_node)
											element = null;
										else
											element = new NodeSelection(new Node[] { focus_node, element_node });
									}
								}
								break;
							}
							case Keys.Alt:
							{
								if (selection != null)
								{
									if (selection.Nodes.Contains(element_node))
									{
										var nodes = selection.Nodes.ToList();
										nodes.Remove(element_node);
										element = new NodeSelection(nodes);
									}
								} else
								{
									var focus_node = FocusElement as Node;
									if (focus_node != null)
										element = null;
								}
								break;
							}
						}
					}
				

					var item = element as NodeItem;
					if (item != null)
					{
						if (!item.OnStartDrag(transformed_location, out originalLocation))
						{
							element = item.Node;
							originalLocation = transformed_location;
						}
					} else
					{
						var connection = element as NodeConnection;
						if (connection != null)
							originalLocation = connection.To.Center;
					}

					// Should compatible connectors be highlighted?
					if (HighlightCompatible && null != CompatibilityStrategy)
					{
						var connectorFrom = element as NodeConnector;
						if (connectorFrom == null)
						{
							var connection = element as NodeConnection;
							if (connection != null)
								connectorFrom = connection.From;
						}
						if (connectorFrom != null)
						{
							if (element.ElementType == ElementType.InputConnector)
							{
								// Iterate over all nodes
								foreach (Node graphNode in graphNodes)
								{
									// Check compatibility of node connectors
									foreach (NodeConnector connectorTo in graphNode.outputConnectors)
									{
                                        var connectionType = CompatibilityStrategy.CanConnect(connectorFrom, connectorTo);
                                        if (connectionType == HyperGraph.Compatibility.ConnectionType.Compatible)
										{
											SetFlag(connectorTo, RenderState.Compatible, true);
                                        }
                                        else if (connectionType == HyperGraph.Compatibility.ConnectionType.Conversion)
                                        {
                                            SetFlag(connectorTo, RenderState.Conversion, true);
                                        } 
                                        else
                                        {
                                            SetFlag(connectorTo, RenderState.Incompatible, true);
                                        }
									}
								}
							} else
							{
								// Iterate over all nodes
								foreach (Node graphNode in graphNodes)
								{
									// Check compatibility of node connectors
									foreach (NodeConnector connectorTo in graphNode.inputConnectors)
									{
                                        var connectionType = CompatibilityStrategy.CanConnect(connectorFrom, connectorTo);
                                        if (connectionType == HyperGraph.Compatibility.ConnectionType.Compatible)
										{
											SetFlag(connectorTo, RenderState.Compatible, true);
                                        }
                                        else if (connectionType == HyperGraph.Compatibility.ConnectionType.Conversion)
                                        {
                                            SetFlag(connectorTo, RenderState.Conversion, true);
                                        }
                                        else
                                        {
                                            SetFlag(connectorTo, RenderState.Incompatible, true);
                                        }
									}
								}
							}
						}
					}

					FocusElement =
						DragElement = element;
					BringElementToFront(element);
					this.Refresh();
					command = CommandMode.Edit;
				} else
					command = CommandMode.MarqueSelection;
			} else
			{
				DragElement = null;
				command = CommandMode.TranslateView;
			}

			points = new PointF[] { originalLocation };
			transformation.TransformPoints(points);
			originalMouseLocation = this.PointToScreen(new Point((int)points[0].X, (int)points[0].Y));
		}
		#endregion

		#region OnMouseMove
		protected override void OnMouseMove(MouseEventArgs e)
		{
			base.OnMouseMove(e);
			
			if (DragElement == null &&
				command != CommandMode.MarqueSelection &&
				(currentButtons & MouseButtons.Right) != 0)
			{
				if (currentButtons == MouseButtons.Right)
					command = CommandMode.TranslateView;
				else
				if (currentButtons == (MouseButtons.Right | MouseButtons.Left))
					command = CommandMode.ScaleView;
			}

			Point currentLocation;
			PointF transformed_location;
			if (abortDrag)
			{
				transformed_location = originalLocation;

				var points = new PointF[] { originalLocation };
				transformation.TransformPoints(points);
				currentLocation = new Point((int)points[0].X, (int)points[0].Y);
			} else
			{
				currentLocation = e.Location;

				var points = new PointF[] { currentLocation };
				inverse_transformation.TransformPoints(points);
				transformed_location = points[0];
			}

			var deltaX = (lastLocation.X - currentLocation.X) / zoom;
			var deltaY = (lastLocation.Y - currentLocation.Y) / zoom;

			

			bool needRedraw = false;
			switch (command)
			{
				case CommandMode.ScaleView:
					if (!mouseMoved)
					{
						if ((Math.Abs(deltaY) > 1))
							mouseMoved = true;
					}

					if (mouseMoved &&
						(Math.Abs(deltaY) > 0))
					{
						zoom *= (float)Math.Pow(2, deltaY / 100.0f);
						Cursor.Position = this.PointToScreen(lastLocation);
						snappedLocation = //lastLocation = 
							currentLocation;
						this.Refresh();
					}
					return;
				case CommandMode.TranslateView:
				{
					if (!mouseMoved)
					{
						if ((Math.Abs(deltaX) > 1) ||
							(Math.Abs(deltaY) > 1))
							mouseMoved = true;
					}

					if (mouseMoved &&
						(Math.Abs(deltaX) > 0) ||
						(Math.Abs(deltaY) > 0))
					{
						translation.X -= deltaX * zoom;
						translation.Y -= deltaY * zoom;
						snappedLocation = lastLocation = currentLocation;
						this.Refresh();
					}
					return;
				}
				case CommandMode.MarqueSelection:
					if (!mouseMoved)
					{
						if ((Math.Abs(deltaX) > 1) ||
							(Math.Abs(deltaY) > 1))
							mouseMoved = true;
					}

					if (mouseMoved &&
						(Math.Abs(deltaX) > 0) ||
						(Math.Abs(deltaY) > 0))
					{
						var marque_rectangle = GetMarqueRectangle();
												
						foreach (var node in selectedNodes)
							SetFlag(node, RenderState.Focus, false, false);

						foreach (var node in unselectedNodes)
							SetFlag(node, RenderState.Focus, true, false);

						if (!abortDrag)
						{
							foreach (var node in graphNodes)
							{
								if (marque_rectangle.Contains(node.bounds))
								{
									if ((node.state & RenderState.Focus) == 0 &&
										(ModifierKeys != Keys.Alt))
									{
										SetFlag(node, RenderState.Focus, true, false);
										selectedNodes.Add(node);
									}
									if ((node.state & RenderState.Focus) != 0 &&
										(ModifierKeys == Keys.Alt))
									{
										SetFlag(node, RenderState.Focus, false, false);
										unselectedNodes.Add(node);
									}
								} else
								{
									if ((node.state & RenderState.Focus) == RenderState.Focus &&
										(ModifierKeys == Keys.None))
									{
										SetFlag(node, RenderState.Focus, false, false);
										unselectedNodes.Add(node);
									}
								}
							}
						}

						snappedLocation = lastLocation = currentLocation;
						this.Refresh();
					}
					return;

				default:
				case CommandMode.Edit:
					break;
			}

			if (dragging)
			{
				if (!mouseMoved)
				{
					if ((Math.Abs(deltaX) > 1) ||
						(Math.Abs(deltaY) > 1))
						mouseMoved = true;
				}

				if (mouseMoved &&
					(Math.Abs(deltaX) > 0) ||
					(Math.Abs(deltaY) > 0))
				{
					mouseMoved = true;
					if (DragElement != null)
					{
						BringElementToFront(DragElement);

						switch (DragElement.ElementType)
						{
							case ElementType.NodeSelection:		// drag nodes
							{
								var selection = DragElement as NodeSelection;
								foreach (var node in selection.Nodes)
								{
									node.Location = new Point(	(int)Math.Round(node.Location.X - deltaX),
																(int)Math.Round(node.Location.Y - deltaY));
								}
								snappedLocation = lastLocation = currentLocation;
								this.Refresh();
								return;
							}
							case ElementType.Node:				// drag single node
							{
								var node = DragElement as Node;
								node.Location	= new Point((int)Math.Round(node.Location.X - deltaX),
															(int)Math.Round(node.Location.Y - deltaY));
								snappedLocation = lastLocation = currentLocation;
								this.Refresh();
								return;
							}
							case ElementType.NodeItem:			// drag in node-item
							{
								var nodeItem = DragElement as NodeItem;
								needRedraw		= nodeItem.OnDrag(transformed_location);
								snappedLocation = lastLocation = currentLocation;
								break;
							}
							case ElementType.Connection:		// start dragging end of connection to new input connector
							{
								BringElementToFront(DragElement);
								var connection			= DragElement as NodeConnection;
								var outputConnector		= connection.From;
								FocusElement			= outputConnector.Node;
								if (Disconnect(connection))
									DragElement	= outputConnector;
								else
									DragElement = null;

								goto case ElementType.OutputConnector;
							}
							case ElementType.InputConnector:	// drag connection from input or output connector
							case ElementType.OutputConnector:
							{
								snappedLocation = lastLocation = currentLocation;
								needRedraw = true;
								break;
							}
						}
					}
				}
			}

			NodeConnector destinationConnector = null;
			IElement draggingOverElement = null;

            SetFlag(DragElement, (RenderState.Compatible | RenderState.Incompatible | RenderState.Conversion), false);
			var element = FindElementAt(transformed_location);
			if (element != null)
			{
				switch (element.ElementType)
				{
					default:
						if (DragElement != null)
							element = null;
						break;

					case ElementType.NodeItem:
					{	
						var item = element as NodeItem;
						if (DragElement != null)
						{
							element = item.Node;
							goto case ElementType.Node;
						}
						break;
					}
					case ElementType.Node:
					{
						var node = element as Node;
						if (DragElement != null)
						{
							if (DragElement.ElementType == ElementType.InputConnector)
							{
								var dragConnector = DragElement as NodeConnector;
								if (dragConnector == null)
									break;
								
								if (node.outputConnectors.Count == 1)
								{
									// Check if this connection would be allowed.
									if (ConnectionIsAllowed(dragConnector, node.outputConnectors[0]))
									{
										element = node.outputConnectors[0];
										goto case ElementType.OutputConnector;
									}
								}
								if (node != dragConnector.Node)
									draggingOverElement = node;
							} else
							if (DragElement.ElementType == ElementType.OutputConnector)
							{
								var dragConnector = DragElement as NodeConnector;
								if (dragConnector == null)
									break;

								if (node.inputConnectors.Count == 1)
								{
									// Check if this connection would be allowed.
									if (ConnectionIsAllowed(dragConnector, node.inputConnectors[0]))
									{
										element = node.inputConnectors[0];
										goto case ElementType.InputConnector;
									}
								}
								if (node != dragConnector.Node)
									draggingOverElement = node;
							}

							//element = null;
						}
						break;
					}
					case ElementType.InputConnector:
					case ElementType.OutputConnector:
					{
						destinationConnector = element as NodeConnector;
						if (destinationConnector == null)
							break;
						
						if (DragElement != null &&
							(DragElement.ElementType == ElementType.InputConnector ||
							 DragElement.ElementType == ElementType.OutputConnector))
						{
							var dragConnector = DragElement as NodeConnector;
							if (dragConnector != null)
							{
								if (dragConnector.Node == destinationConnector.Node ||
									DragElement.ElementType == element.ElementType)
								{
									element = null;
								} else
								{
									if (!ConnectionIsAllowed(dragConnector, destinationConnector))
									{
										SetFlag(DragElement, RenderState.Incompatible, true);
									} else
									{
                                        SetFlag(DragElement, (destinationConnector.state & (RenderState.Compatible | RenderState.Incompatible | RenderState.Conversion)), true);
									}
								}
							}
						}
						draggingOverElement = destinationConnector.Node;
						break;
					}
				}
			}
		
			if (HoverElement != element)
			{
				HoverElement = element;
				needRedraw = true;
			}

			if (internalDragOverElement != draggingOverElement)
			{
				if (internalDragOverElement != null)
				{
					SetFlag(internalDragOverElement, RenderState.DraggedOver, false);
					var node = GetElementNode(internalDragOverElement);
					if (node != null)
						GraphRenderer.PerformLayout(this.CreateGraphics(), node);
					needRedraw = true;
				}

				internalDragOverElement = draggingOverElement;

				if (internalDragOverElement != null)
				{
					SetFlag(internalDragOverElement, RenderState.DraggedOver, true);
					var node = GetElementNode(internalDragOverElement);
					if (node != null)
						GraphRenderer.PerformLayout(this.CreateGraphics(), node);
					needRedraw = true;
				}
			}

			if (destinationConnector != null)
			{
				if (!destinationConnector.bounds.IsEmpty)
				{
					// var pre_points = new PointF[] { 
					// 	new PointF((destinationConnector.bounds.Left + destinationConnector.bounds.Right) / 2,
					// 				(destinationConnector.bounds.Top  + destinationConnector.bounds.Bottom) / 2) };
                    float width = destinationConnector.bounds.Bottom - destinationConnector.bounds.Top - 8; 
                    var pre_points = new PointF[] { 
					 	new PointF( destinationConnector.bounds.Left + width / 2.0f + 4.0f,
					 				(destinationConnector.bounds.Top  + destinationConnector.bounds.Bottom) / 2) };
					transformation.TransformPoints(pre_points);
					snappedLocation = pre_points[0];
				}
			}
						

			if (needRedraw)
				this.Refresh();
		}

		/// <summary>
		/// Checks whether the connection between two connectors is allowed.
		/// This is achieved through event propagation.
		/// </summary>
		/// <returns></returns>
		private bool ConnectionIsAllowed(NodeConnector from, NodeConnector to)
		{
			if (HighlightCompatible && null != CompatibilityStrategy)
			{
				if (CompatibilityStrategy.CanConnect(from, to) == ConnectionType.Incompatible)
					return false;
			}
			
			// If someone has subscribed to the ConnectionAdding event,
			// give them a chance to interrupt this connection attempt.
			if (null != ConnectionAdding) 
			{
				// Populate a temporary NodeConnection instance.
				var connection = new NodeConnection();
				connection.From = from;
				connection.To = to;

				// Fire the event and see if someone cancels it.
				var eventArgs = new AcceptNodeConnectionEventArgs(connection);
				ConnectionAdding(this, eventArgs);
				if (eventArgs.Cancel)
					return false;
			}
			return true;
		}

		#endregion

		#region GetElementNode
		private Node GetElementNode(IElement element)
		{
			if (element == null)
				return null;
			switch (element.ElementType)
			{
				default:
				case ElementType.Connection:		return null;
				case ElementType.InputConnector:	return ((NodeInputConnector)element).Node;
				case ElementType.OutputConnector:	return ((NodeInputConnector)element).Node;
				case ElementType.NodeItem:			return ((NodeItem)element).Node;
				case ElementType.Node:				return (Node)element;
			}
		}
		#endregion

        public sealed class NodeConnectorEventArgs : EventArgs
        {
            public NodeConnector Connector { get; set; }
            public Node Node { get; set; }
        }
        public event EventHandler<NodeConnectorEventArgs> ConnectorDoubleClick;

		#region OnMouseUp
		protected override void OnMouseUp(MouseEventArgs e)
		{
			currentButtons &= ~e.Button;
			
			bool needRedraw = false;
			if (!dragging)
				return;
			
			try
			{
				Point currentLocation;
				PointF transformed_location;
				if (abortDrag)
				{
					transformed_location = originalLocation;

					var points = new PointF[] { originalLocation };
					transformation.TransformPoints(points);
					currentLocation = new Point((int)points[0].X, (int)points[0].Y);
				} else
				{
					currentLocation = e.Location;

					var points = new PointF[] { currentLocation };
					inverse_transformation.TransformPoints(points);
					transformed_location = points[0];
				}


				switch (command)
				{
					case CommandMode.MarqueSelection:
						if (abortDrag)
						{
							foreach (var node in selectedNodes)
								SetFlag(node, RenderState.Focus, false, false);

							foreach (var node in unselectedNodes)
								SetFlag(node, RenderState.Focus, true, false);							
						} else
						{
							NodeSelection selection = null;
							if (graphNodes.Count > 0)
							{
								// select all focused nodes
								var result = (from node in graphNodes
											  where (node.state & RenderState.Focus) == RenderState.Focus
											  select node).ToList();
								if (result.Count > 0)
									selection = new NodeSelection(result);
							}
							FocusElement = selection;
						}
						this.Invalidate();
						return;
					case CommandMode.ScaleView:
						return;
					case CommandMode.TranslateView:
						return;

					default:
					case CommandMode.Edit:
						break;
				}
				if (DragElement != null)
				{
					switch (DragElement.ElementType)
					{
						case ElementType.InputConnector:
						{
							var inputConnector	= (NodeConnector)DragElement;
							var outputConnector = HoverElement as NodeOutputConnector;
							if (outputConnector != null &&
								outputConnector.Node != inputConnector.Node &&
								(inputConnector.state & (RenderState.Compatible|RenderState.Conversion)) != 0)
								FocusElement = Connect(outputConnector, inputConnector);
							needRedraw = true;
							return;
						}
						case ElementType.OutputConnector:
						{
							var outputConnector = (NodeConnector)DragElement;
							var inputConnector	= HoverElement as NodeInputConnector;
							if (inputConnector != null &&
								inputConnector.Node != outputConnector.Node &&
                                (outputConnector.state & (RenderState.Compatible | RenderState.Conversion)) != 0)
								FocusElement = Connect(outputConnector, inputConnector);
							needRedraw = true;
							return;
						}
						default:
						case ElementType.NodeSelection:
						case ElementType.Connection:
						case ElementType.NodeItem:
						case ElementType.Node:
						{
							needRedraw = true;
							return;
						}
					}
				}
				if (DragElement != null ||
					FocusElement != null)
				{
					FocusElement = null;
					needRedraw = true;
				}
			}
			finally
			{
				if (HighlightCompatible)
				{
					// Remove all highlight flags
					foreach (Node graphNode in graphNodes)
					{
						foreach (NodeConnector inputConnector in graphNode.inputConnectors)
                            SetFlag(inputConnector, RenderState.Compatible | RenderState.Incompatible | RenderState.Conversion, false);

						foreach (NodeConnector outputConnector in graphNode.outputConnectors)
                            SetFlag(outputConnector, RenderState.Compatible | RenderState.Incompatible | RenderState.Conversion, false);
					}
				}

				if (DragElement != null)
				{
					var nodeItem = DragElement as NodeItem;
					if (nodeItem != null)
						nodeItem.OnEndDrag();
					DragElement = null;
					needRedraw = true;
				}

				dragging = false;
				command = CommandMode.Edit;
				selectedNodes.Clear();
				unselectedNodes.Clear();
				
				if (needRedraw)
					this.Refresh();
			
				base.OnMouseUp(e);
			}
		}
		#endregion

		#region OnDoubleClick
		bool ignoreDoubleClick = false;
		protected override void OnDoubleClick(EventArgs e)
		{
			base.OnDoubleClick(e);
			if (mouseMoved || ignoreDoubleClick || 
				ModifierKeys != Keys.None)
				return;

			var points = new Point[] { lastLocation };
			inverse_transformation.TransformPoints(points);
			var transformed_location = points[0];

			var element = FindElementAt(transformed_location);
			if (element == null)
				return;

			switch (element.ElementType)
			{
				case ElementType.Connection:
					((NodeConnection)element).DoDoubleClick();
					break;
				case ElementType.NodeItem:
					var item = element as NodeItem;
					if (item.OnDoubleClick(this))
					{
						this.Refresh();
						return;
					}
					element = item.Node;
					goto case ElementType.Node;
				case ElementType.Node:
					var node = element as Node;
					node.Collapsed = !node.Collapsed;
					FocusElement = node;
					this.Refresh();
					break;

                case ElementType.InputConnector:
                case ElementType.OutputConnector:
                    if (ConnectorDoubleClick != null)
                        ConnectorDoubleClick(this, new NodeConnectorEventArgs() { Connector = (NodeConnector)element, Node = ((NodeConnector)element).Node });
                    break;
			}
		}
		#endregion

		#region OnMouseClick
		protected override void OnMouseClick(MouseEventArgs e)
		{
			try
			{
				ignoreDoubleClick = false;
				if (mouseMoved)
					return;

				var points = new Point[] { lastLocation };
				inverse_transformation.TransformPoints(points);
				var transformed_location = points[0];

				if (e.Button == MouseButtons.Right)
				{
					if (null != ShowElementMenu)
					{
						// See if we clicked on an element and give our owner the chance to show a menu
						var result = FindElementAt(transformed_location, delegate(IElement el)
						{
							// Fire the event and see if someone cancels it.
							var eventArgs = new AcceptElementLocationEventArgs(el, this.PointToScreen(lastLocation));
							// Give our owner the chance to show a menu for this element ...
							ShowElementMenu(this, eventArgs);
							// If the owner declines (cancel == true) then we'll continue looking up the hierarchy ..
							return !eventArgs.Cancel;
						});
						// If we haven't found anything to click on we'll just return the event with a null pointer .. 
						//	allowing our owner to show a generic menu
						if (result == null)
						{
							var eventArgs = new AcceptElementLocationEventArgs(null, this.PointToScreen(lastLocation));
							ShowElementMenu(this, eventArgs);							
						}
						return;
					}
				}

				var element = FindElementAt(transformed_location);
				if (element == null)
				{
					ignoreDoubleClick = true; // to avoid double-click from firing
					if (ModifierKeys == Keys.None)
						FocusElement = null;
					return;
				}

				switch (element.ElementType)
				{
					case ElementType.NodeItem:
					{
						if (ModifierKeys != Keys.None)
							return;

						var item = element as NodeItem;
						if (item.OnClick(this, e, transformation))
						{
							ignoreDoubleClick = true; // to avoid double-click from firing
							this.Refresh();
							return;
						}
						break;
					}
				}
			}
			finally
			{
				base.OnMouseClick(e);
			}
		}
		#endregion

		#region OnKeyDown
		protected override void OnKeyDown(KeyEventArgs e)
		{
			base.OnKeyDown(e);
			if (e.KeyCode == Keys.Escape)
			{
				if (dragging)
				{
					abortDrag = true;
					if (command == CommandMode.Edit)
					{
						Cursor.Position = originalMouseLocation;
					} else
					if (command == CommandMode.MarqueSelection)
					{
						foreach (var node in selectedNodes)
							SetFlag(node, RenderState.Focus, false, false);
						
						foreach (var node in unselectedNodes)
							SetFlag(node, RenderState.Focus, true, false);

						this.Refresh();
					}
					return;
				}
			}
		}
		#endregion

		#region OnKeyUp
		protected override void OnKeyUp(KeyEventArgs e)
		{
			base.OnKeyUp(e);
			if (e.KeyCode == Keys.Delete)
			{
				if (FocusElement == null)
					return;

				switch (FocusElement.ElementType)
				{
					case ElementType.Node:			RemoveNode(FocusElement as Node); break;
					case ElementType.Connection:	Disconnect(FocusElement as NodeConnection); break;
					case ElementType.NodeSelection:
					{
						var selection = FocusElement as NodeSelection;
						foreach(var node in selection.Nodes)
							RemoveNode(node); 
						break;
					}
				}
			}
		}
		#endregion


		#region OnDragEnter
		Node dragNode = null;
		protected override void OnDragEnter(DragEventArgs drgevent)
		{
			base.OnDragEnter(drgevent);
			dragNode = null;

			foreach (var name in drgevent.Data.GetFormats())
			{
				var node = drgevent.Data.GetData(name) as Node;
				if (node != null)
				{
					if (AddNode(node))
					{
						dragNode = node;

						drgevent.Effect = DragDropEffects.Copy;
					}
					return;
				}
			}
		}
		#endregion

		#region OnDragOver
		protected override void OnDragOver(DragEventArgs drgevent)
		{
			base.OnDragOver(drgevent);
			if (dragNode == null)
				return;

			var location = (PointF)this.PointToClient(new Point(drgevent.X, drgevent.Y));
			location.X -= ((dragNode.bounds.Right - dragNode.bounds.Left) / 2);
			location.Y -= ((dragNode.titleItem.bounds.Bottom - dragNode.titleItem.bounds.Top) / 2);
			
			var points = new PointF[] { location };
			inverse_transformation.TransformPoints(points);
			location = points[0];

			if (dragNode.Location != location)
			{
				dragNode.Location = location;
				this.Invalidate();
			}
			
			drgevent.Effect = DragDropEffects.Copy;
		}
		#endregion

		#region OnDragLeave
		protected override void OnDragLeave(EventArgs e)
		{
			base.OnDragLeave(e);
			if (dragNode == null)
				return;
			RemoveNode(dragNode);
			dragNode = null;
		}
		#endregion

		#region OnDragDrop
		protected override void OnDragDrop(DragEventArgs drgevent)
		{
			base.OnDragDrop(drgevent);
		}
		#endregion
	}
}
