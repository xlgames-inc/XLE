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

        public System.Drawing.RectangleF bounds
        {
            get
            {
                float minX = System.Single.MaxValue, minY = System.Single.MaxValue;
                float maxX = System.Single.MinValue, maxY = System.Single.MinValue;
                foreach (var n in Nodes)
                {
                    minX = System.Math.Min(minX, n.bounds.Left);
                    minY = System.Math.Min(minY, n.bounds.Top);
                    maxX = System.Math.Max(maxX, n.bounds.Right);
                    maxY = System.Math.Max(maxY, n.bounds.Bottom);
                }
                if (maxX >= minX && maxY >= minY)
                    return new System.Drawing.RectangleF() { X = minX, Y = minY, Width = maxX - minX, Height = maxY - minY };
                return System.Drawing.RectangleF.Empty;
            }
        }
    }
}
