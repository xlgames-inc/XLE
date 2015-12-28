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

        public ICompatibilityStrategy CompatibilityStrategy { get; set; }

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
                        foreach (var node in selection.Nodes.Reverse<Node>())
                        {
                            if (_graphNodes[0] != node)
                            {
                                _graphNodes.Remove(node);
                                _graphNodes.Insert(0, node);
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
            if (modified)
                InvalidateViews(this, EventArgs.Empty);
            return modified;
        }
        #endregion

        #region Connect / Disconnect
        public NodeConnection Connect(NodeItem from, NodeItem to)
        {
            return Connect(from.Output, to.Input);
        }

        public NodeConnection Connect(NodeConnector from, NodeConnector to)
        {
            if (from == null || to == null ||
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
    }
}
