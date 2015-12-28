using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Text;

namespace HyperGraph
{
    public sealed class NodeEventArgs : EventArgs
    {
        public NodeEventArgs(Node node) { Node = node; }
        public Node Node { get; private set; }
    }

    public sealed class ElementEventArgs : EventArgs
    {
        public ElementEventArgs(IElement element) { Element = element; }
        public IElement Element { get; private set; }
    }

    public sealed class AcceptNodeEventArgs : CancelEventArgs
    {
        public AcceptNodeEventArgs(Node node) { Node = node; }
        public AcceptNodeEventArgs(Node node, bool cancel) : base(cancel) { Node = node; }
        public Node Node { get; private set; }
    }

    public sealed class AcceptElementLocationEventArgs : CancelEventArgs
    {
        public AcceptElementLocationEventArgs(IElement element, Point position) { Element = element; Position = position; }
        public AcceptElementLocationEventArgs(IElement element, Point position, bool cancel) : base(cancel) { Element = element; Position = position; }
        public IElement Element { get; private set; }
        public Point Position { get; private set; }
    }

    public interface IGraphModel
    {
        IEnumerable<Node> Nodes { get; }

        void BringElementToFront(IElement element);
        bool AddNode(Node node);
        bool AddNodes(IEnumerable<Node> nodes);
        void RemoveNode(Node node);
        bool RemoveNodes(IEnumerable<Node> nodes);

        NodeConnection Connect(NodeItem from, NodeItem to);
        NodeConnection Connect(NodeConnector from, NodeConnector to);
        bool Disconnect(NodeConnection connection);
        bool DisconnectAll(Node node);
        bool ConnectionIsAllowed(NodeConnector from, NodeConnector to);

        Compatibility.ICompatibilityStrategy CompatibilityStrategy { get; set; }

        event EventHandler<AcceptNodeEventArgs>              NodeAdded;
        event EventHandler<AcceptNodeEventArgs>              NodeRemoving;
        event EventHandler<NodeEventArgs>                    NodeRemoved;
        event EventHandler<AcceptNodeConnectionEventArgs>    ConnectionAdding;
        event EventHandler<AcceptNodeConnectionEventArgs>    ConnectionAdded;
        event EventHandler<AcceptNodeConnectionEventArgs>    ConnectionRemoving;
        event EventHandler<NodeConnectionEventArgs>          ConnectionRemoved;
        event EventHandler<EventArgs>                        InvalidateViews;
    }
}
