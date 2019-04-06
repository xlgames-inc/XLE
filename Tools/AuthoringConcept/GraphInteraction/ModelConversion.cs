using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Text;
using HyperGraph;
using HyperGraph.Compatibility;
using HyperGraph.Items;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace AuthoringConcept
{
    public class EditorFrameItem : HyperGraph.NodeItem
    {
        #region Drawing & Measuring
        public override SizeF Measure(Graphics context)
        {
            return _guiFrame.Measure(context, new SizeF(256.0f + 128.0f, 64.0f));
        }

        public override void Render(Graphics graphics, RectangleF rectangle, object context)
        {
            _guiFrame.Draw(graphics, rectangle);
        }
        #endregion

        #region IO
        public override bool OnClick(Control container, MouseEventArgs evnt, Matrix viewTransform)
        {
            Point[] transformables = { new Point(evnt.X, evnt.Y) };
            var inverseViewTransform = viewTransform.Clone();
            inverseViewTransform.Invert();
            inverseViewTransform.TransformPoints(transformables);

            var localEvnt = new MouseEventArgs(
                evnt.Button,
                evnt.Clicks,
                transformables[0].X - (int)GetBounds().X,
                transformables[0].Y - (int)GetBounds().Y,
                evnt.Delta);

            // emulate both mouse down & up events
            _guiFrame.OnMouseDown(localEvnt);
            _guiFrame.OnMouseUp(localEvnt);
            return true;
        }

        public override bool OnDoubleClick(Control container)
        {
            return false;
        }

        public override bool OnStartDrag(PointF location, out PointF original_location) { original_location = Point.Empty; return false; }
        public override bool OnDrag(PointF location) { return false; }
        public override bool OnEndDrag() { return false; }
        #endregion

        public EditorFrameItem(AdaptiveEditing.EditorFrame frame)
        {
            _guiFrame = frame;
        }

        private AdaptiveEditing.EditorFrame _guiFrame;
    }

    public static class TestNodeFactory
    {
        public class NodeTag
        {
            public UInt32 Id { get; set; }
            public string DataBlockId { get; set; }
            public string DataBlockType { get; set; }
        }

        public class SubGraphNodeTag : NodeTag
        {
            public Sce.Atf.Dom.DomNode Node;
        }

        private static UInt32 static_nextNodeId = 1;

        public static HyperGraph.Node CreateSubGraph(String name, String implements)
        {
            var node = new HyperGraph.Node { };
            node.Tag = new SubGraphNodeTag();
            node.SubGraphTag = name;
            node.AddItem(new HyperGraph.Items.NodeTextBoxItem(name), HyperGraph.Node.Dock.Top);
            node.AddItem(new HyperGraph.Items.NodeTitleItem { Title = " implements " }, HyperGraph.Node.Dock.Top);
            node.AddItem(new HyperGraph.Items.NodeTextBoxItem(implements), HyperGraph.Node.Dock.Top);
            return node;
        }

        public static string GetDescription(object o)
        {
            var node = o as HyperGraph.Node;
            if (node != null)
            {
                return node.Title;
            }

            return string.Empty;
        }

        public static HyperGraph.Node FindNodeFromId(HyperGraph.IGraphModel graph, UInt64 id)
        {
            foreach (HyperGraph.Node n in graph.Nodes)
            {
                if (n.Tag is NodeTag
                    && ((NodeTag)n.Tag).Id == (UInt64)id)
                {
                    return n;
                }
            }
            return null;
        }

        public static HyperGraph.Node CreateTestNode(AdaptiveEditing.IDataBlockDeclaration decl, AdaptiveEditing.IDataBlock storage)
        {
            var node = new HyperGraph.Node();
            node.Tag = new NodeTag { Id = static_nextNodeId, DataBlockId = "Node_" + static_nextNodeId, DataBlockType = storage.TypeIdentifier };
            ++static_nextNodeId;
            node.AddItem(new HyperGraph.Items.NodeTitleItem { Title = "Title" }, HyperGraph.Node.Dock.Top);

            node.AddItem(new TestNodeConnector("Input0", "float"), HyperGraph.Node.Dock.Input);
            node.AddItem(new TestNodeConnector("Input1", "float"), HyperGraph.Node.Dock.Input);
            node.AddItem(new TestNodeConnector("Output", "float"), HyperGraph.Node.Dock.Output);
            var frame = new AdaptiveEditing.EditorFrame { Declaration = decl, Storage = storage };
            node.AddItem(new EditorFrameItem(frame), HyperGraph.Node.Dock.Center);
            return node;
        }

        internal class TestNodeConnector : HyperGraph.NodeConnector
        {
            public TestNodeConnector(string name, string type)
            {
                this.Name = name;
                this.Type = type;
            }

            #region Properties
            string internalText = string.Empty;
            public string Name
            {
                get { return internalText; }
                set
                {
                    if (internalText == value)
                        return;
                    internalText = value;
                    TextSize = Size.Empty;
                }
            }
            internal string _shortType;
            internal string _typeText;
            public string Type
            {
                get { return _typeText; }
                set
                {
                    _typeText = value;
                    if (value != null && value.Length > 0)
                    {
                        _shortType = value[0].ToString();
                        if (value.Count() > 0 && Char.IsDigit(value[value.Count() - 1]))
                        {
                            if (value.Count() > 2 && value[value.Count() - 2] == 'x' && Char.IsDigit(value[value.Count() - 3]))
                            {
                                _shortType += value.Substring(value.Count() - 3);
                            }
                            else
                            {
                                _shortType += value.Substring(value.Count() - 1);
                            }
                        }
                    }
                    else
                    {
                        _shortType = "";
                    }
                    Tag = value;
                }
            }
            public string ShortType { get { return _shortType; } }
            internal SizeF TextSize;
            #endregion

            public override SizeF Measure(Graphics graphics)
            {
                // Resize based on the length of the strings. Sometimes we get really
                // long names, so it's useful to resize the connector to match...!
                var mainStringSize = graphics.MeasureString(GetNameText(), SystemFonts.MenuFont);
                var shortTypeSize = graphics.MeasureString(GetTypeText(), SystemFonts.MenuFont);
                const uint separation = 8;
                return new SizeF(
                    mainStringSize.Width + separation + shortTypeSize.Width,
                    Math.Max(HyperGraph.GraphConstants.MinimumItemHeight, mainStringSize.Height));
            }

            public override void Render(Graphics graphics, RectangleF bounds, object context)
            {
                graphics.DrawString(
                    this.Name, SystemFonts.MenuFont, Brushes.White,
                    bounds, HyperGraph.GraphConstants.LeftTextStringFormat);

                var mainStringSize = graphics.MeasureString(GetNameText(), SystemFonts.MenuFont);
                const uint separation = 8;

                RectangleF newRect = new RectangleF(bounds.Location, bounds.Size);
                newRect.X += mainStringSize.Width + separation;
                newRect.Width -= mainStringSize.Width + separation;
                graphics.DrawString(GetTypeText(), SystemFonts.MenuFont, Brushes.DarkGray,
                    newRect, HyperGraph.GraphConstants.LeftTextStringFormat);
            }

            internal virtual string GetNameText() { return this.Name; }
            internal virtual string GetTypeText() { return "(" + this.ShortType + ")"; }
        }
    }

    public static class ModelConversion
    {
        private static string UnplacedSubGraphTag = "unplaced";

            //
            //      Convert from the "ViewModel" to the "Model"
            //
            //      We don't maintain the GUILayer.NodeGraph representation
            //      permanently... But we need this for serialization and shader
            //      generation operations
            //
            //      So, let's just build it from the graph control object.
            //
        public static GUILayer.NodeGraphFile ToNodeGraphFile(HyperGraph.IGraphModel graph, bool includeEditorAttributes)
        {
            GUILayer.NodeGraphFile result = new GUILayer.NodeGraphFile();
            foreach (Node n in graph.SubGraphs)
            {
                GUILayer.NodeGraphSignature signature = new GUILayer.NodeGraphSignature();
                result.SubGraphs.Add(
                    n.SubGraphTag as string,
                    new GUILayer.NodeGraphFile.SubGraph
                    {
                        Graph = new GUILayer.NodeGraph(),
                        Signature = signature
                    });
            }

            int visualNodeId = 0;
            Dictionary<Node, int> nodeToVisualNodeId = new Dictionary<Node, int>();
            foreach (Node n in graph.Nodes)
            {
                var nTag = n.Tag as TestNodeFactory.NodeTag;
                if (nTag == null) continue;

                GUILayer.NodeGraphFile.SubGraph resultSubGraph = null;
                if ((n.SubGraphTag as string) != null)
                    result.SubGraphs.TryGetValue(n.SubGraphTag as string, out resultSubGraph);

                if (resultSubGraph == null)
                    result.SubGraphs.TryGetValue(UnplacedSubGraphTag, out resultSubGraph);

                if (resultSubGraph == null)
                {
                    resultSubGraph = new GUILayer.NodeGraphFile.SubGraph
                    {
                        Graph = new GUILayer.NodeGraph(),
                        Signature = new GUILayer.NodeGraphSignature()
                    };
                    result.SubGraphs.Add(UnplacedSubGraphTag, resultSubGraph);
                }

                {
                    GUILayer.Node resultNode = new GUILayer.Node {
                        FragmentArchiveName = nTag.DataBlockId + "<" + nTag.DataBlockType + ">", NodeId = nTag.Id };
                    resultSubGraph.Graph.AddNode(resultNode);

                    if (includeEditorAttributes)
                    {
                        string attributeTableName = "visualNode" + visualNodeId; ++visualNodeId;
                        resultNode.AttributeTableName = attributeTableName;

                        var attributeTable = new Dictionary<string, string>();
                        attributeTable.Add("X", n.Location.X.ToString());
                        attributeTable.Add("Y", n.Location.Y.ToString());
                        attributeTable.Add("State", n.Collapsed ? "Collapsed" : "Normal");
                        result.AttributeTables.Add(attributeTableName, attributeTable);
                    }
                }
            }

            foreach(var sg in result.SubGraphs)
            {
                HashSet<NodeConnection> connectionSet = new HashSet<NodeConnection>();
                foreach (Node n in graph.Nodes.Concat(graph.SubGraphs))
                    if (n.SubGraphTag == sg.Key)
                        foreach (NodeConnection c in n.Connections)
                            connectionSet.Add(c);

                var resultSubGraph = sg.Value;

                // Build connections...
                foreach (NodeConnection connection in connectionSet)
                {
                    var dstNode = (connection.To != null) ? connection.To.Node.Tag as TestNodeFactory.NodeTag : null;
                    var srcNode = (connection.From != null) ? connection.From.Node.Tag as TestNodeFactory.NodeTag : null;

                    if (dstNode != null && srcNode != null)
                    {
                        resultSubGraph.Graph.AddConnection(
                            new GUILayer.Connection()
                                {
                                    OutputNodeID = dstNode.Id,
                                    OutputParameterName = ((TestNodeFactory.TestNodeConnector)connection.To).Name,
                                    InputNodeID = srcNode.Id,
                                    InputParameterName = ((TestNodeFactory.TestNodeConnector)connection.From).Name,
                                    Condition = connection.Text
                                });
                    }
                }
            }

            return result;
        }
    }
}
