using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HyperGraph
{
	public class NodeSelection : IElement
	{
		public NodeSelection(IEnumerable<Node> nodes) { Nodes = nodes.ToArray(); }
		public ElementType ElementType { get { return ElementType.NodeSelection; } }
		public readonly Node[] Nodes;
	}
}
