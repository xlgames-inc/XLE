using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using HyperGraph.Compatibility;

namespace HyperGraph
{
    public class GraphModel : IGraphModel
    {
        public IEnumerable<Node> Nodes
        {
            get { return _graphNodes; }
        }

        private readonly List<Node> _graphNodes = new List<Node>();
        private static uint _nextRevisionIndex = 1;
        private uint _revisionIndex;

        public ICompatibilityStrategy CompatibilityStrategy { get; set; }

        #region BringElementToFront
        public void BringElementToFront(IElement element)
        {
            if (element == null)
                return;

            bool madeChange = false;
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
                        madeChange = true;
                    }

                    connections = connection.To.Node.connections;
                    if (connections[0] != connection)
                    {
                        connections.Remove(connection);
                        connections.Insert(0, connection);
                        madeChange = true;
                    }
                    break;
                case ElementType.NodeSelection:
                    {
                        var selection = element as NodeSelection;
                        foreach (var node in selection.Nodes.Reverse<Node>())
                        {
                            if (_graphNodes[0] != node)
                            {
                                _graphNodes.Remove(node);
                                _graphNodes.Insert(0, node);
                                madeChange = true;
                            }
                        }
                        break;
                    }
                case ElementType.Node:
                    {
                        var node = element as Node;
                        if (_graphNodes[0] != node)
                        {
                            _graphNodes.Remove(node);
                            _graphNodes.Insert(0, node);
                            madeChange = true;
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

            if (madeChange)
            {
                // don't call UpdateRevisionIndex(); here (generated shaders should not changed)
                if (InvalidateViews != null)
                    InvalidateViews(this, EventArgs.Empty);
            }
        }
        #endregion

        #region Add / Remove
        public bool AddNode(Node node)
        {
            if (node == null ||
                _graphNodes.Contains(node))
                return false;

            _graphNodes.Insert(0, node);
            if (NodeAdded != null)
            {
                var eventArgs = new AcceptNodeEventArgs(node);
                NodeAdded(this, eventArgs);
                if (eventArgs.Cancel)
                {
                    _graphNodes.Remove(node);
                    return false;
                }
            }

            BringElementToFront(node);
            // FocusElement = node;
            UpdateRevisionIndex();
            if (InvalidateViews != null)
                InvalidateViews(this, EventArgs.Empty);
            return true;
        }

        public bool AddNodes(IEnumerable<Node> nodes)
        {
            if (nodes == null)
                return false;

            int index = 0;
            bool modified = false;
            Node lastNode = null;
            foreach (var node in nodes)
            {
                if (node == null)
                    continue;
                if (_graphNodes.Contains(node))
                    continue;

                _graphNodes.Insert(index, node); index++;

                if (NodeAdded != null)
                {
                    var eventArgs = new AcceptNodeEventArgs(node);
                    NodeAdded(this, eventArgs);
                    if (eventArgs.Cancel)
                    {
                        _graphNodes.Remove(node);
                        modified = true;
                    }
                    else
                        lastNode = node;
                }
                else
                    lastNode = node;
            }
            if (lastNode != null)
            {
                BringElementToFront(lastNode);
                // FocusElement = lastNode;
                UpdateRevisionIndex();
                if (InvalidateViews != null) 
                    InvalidateViews(this, EventArgs.Empty);
            }
            return modified;
        }

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
            // if (HasFocus(node))
            //     FocusElement = null;

            DisconnectAll(node);
            _graphNodes.Remove(node);
            UpdateRevisionIndex();
            if (InvalidateViews != null) 
                InvalidateViews(this, EventArgs.Empty);

            if (NodeRemoved != null)
                NodeRemoved(this, new NodeEventArgs(node));
        }

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

                // if (HasFocus(node))
                //     FocusElement = null;

                DisconnectAll(node);
                _graphNodes.Remove(node);
                modified = true;

                if (NodeRemoved != null)
                    NodeRemoved(this, new NodeEventArgs(node));
            }
            if (modified) {
                UpdateRevisionIndex();
                if (InvalidateViews != null) 
                    InvalidateViews(this, EventArgs.Empty);
            }
            return modified;
        }
        #endregion

        #region Connect / Disconnect
        public NodeConnection Connect(NodeItem from, NodeItem to, string name)
        {
            return Connect(from.Output, to.Input, name);
        }

        public NodeConnection Connect(NodeConnector from, NodeConnector to, string name)
        {
            if (from != null)
            {
                foreach (var other in from.Node.connections)
                {
                    if (other.From == from &&
                        other.To == to)
                        return null;
                }
            }

            if (to != null)
            {
                foreach (var other in to.Node.connections)
                {
                    if (other.From == from &&
                        other.To == to)
                        return null;
                }
            }

            var connection = new NodeConnection();
            connection.From = from;
            connection.To = to;
            connection.Name = name;

            if (from != null)
                from.Node.connections.Add(connection);
            if (to != null)
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

            UpdateRevisionIndex();
            if (InvalidateViews != null) 
                InvalidateViews(this, EventArgs.Empty);

            return connection;
        }

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

            // if (HasFocus(connection))
            //     FocusElement = null;

            var from = connection.From;
            var to = connection.To;
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

            UpdateRevisionIndex();
            if (InvalidateViews != null) 
                InvalidateViews(this, EventArgs.Empty);
            return true;
        }

        public bool DisconnectAll(Node node)
        {
            bool modified = false;
            var connections = node.connections.ToList();
            foreach (var connection in connections)
                modified = Disconnect(connection) ||
                    modified;
            return modified;
        }

        public bool ConnectionIsAllowed(NodeConnector from, NodeConnector to)
        {
            if (null != CompatibilityStrategy)
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

        #region Events
        public event EventHandler<AcceptNodeEventArgs> NodeAdded;
        public event EventHandler<AcceptNodeEventArgs> NodeRemoving;
        public event EventHandler<NodeEventArgs> NodeRemoved;
        public event EventHandler<AcceptNodeConnectionEventArgs> ConnectionAdding;
        public event EventHandler<AcceptNodeConnectionEventArgs> ConnectionAdded;
        public event EventHandler<AcceptNodeConnectionEventArgs> ConnectionRemoving;
        public event EventHandler<NodeConnectionEventArgs> ConnectionRemoved;
        public event EventHandler<EventArgs> InvalidateViews;
        #endregion

        public GraphModel() { UpdateRevisionIndex(); }

        private void UpdateRevisionIndex() { _revisionIndex = _nextRevisionIndex++; }
        public uint GlobalRevisionIndex { get { return _revisionIndex; } }
    }

    public class GraphSelection : IGraphSelection
    {
        public ISet<IElement> Selection { get { return _selection; } }
        private readonly HashSet<IElement> _selection = new HashSet<IElement>();

        public void Update(IEnumerable<IElement> selectedItems, IEnumerable<IElement> deselectedItems)
        {
            // If there is no actual change, then we should abort immediately without
            // invoking our event handlers.
            bool change = false;
            change |= (deselectedItems != null && _selection.Overlaps(deselectedItems));
            change |= (selectedItems != null && !_selection.IsSupersetOf(selectedItems));
            if (!change) return;

            if (SelectionChanging != null)
                SelectionChanging.Invoke(this, EventArgs.Empty);
            if (deselectedItems != null)
                _selection.ExceptWith(deselectedItems);
            if (selectedItems != null)
                _selection.UnionWith(selectedItems);
            if (SelectionChanged != null)
                SelectionChanged.Invoke(this, EventArgs.Empty);
        }

        public void SelectSingle(IElement newSelection)
        {
            if (SelectionChanging != null)
                SelectionChanging.Invoke(this, EventArgs.Empty);
            _selection.Clear();
            _selection.Add(newSelection);
            if (SelectionChanged != null)
                SelectionChanged.Invoke(this, EventArgs.Empty);
        }

        public event EventHandler SelectionChanged; 
        public event EventHandler SelectionChanging;

        public bool Contains(IElement element)
        {
            if (element == null)
				return false;

            foreach (var e in Selection) {
			    if (element.ElementType ==
				    e.ElementType)
				    return (element == e);
			
			    switch (e.ElementType)
			    {
				    case ElementType.Connection:
					    var focusConnection = e as NodeConnection;
					    return (focusConnection.To == element ||
							    focusConnection.From == element ||
							
							    ((focusConnection.To != null &&
							    focusConnection.To.Node == element) ||
							    (focusConnection.From != null &&
							    focusConnection.From.Node == element)));
				    case ElementType.NodeItem:
					    var focusItem = e as NodeItem;
					    return (focusItem.Node == element);
				    case ElementType.InputConnector:
				    case ElementType.OutputConnector:
					    var focusConnector = e as NodeConnector;
					    return (focusConnector.Node == element);
				    case ElementType.NodeSelection:
				    {
					    var selection = e as NodeSelection;
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

            return false;
        }
    }
}
