// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using HyperGraph;
using HyperGraph.Compatibility;
using HyperGraph.Items;

namespace NodeEditor
{
    class ModelConversion
    {
        private static ShaderPatcherLayer.Node.Type AsNodeType(ShaderFragmentArchive.Parameter.SourceType input)
        {
            switch (input)
            {
                case ShaderFragmentArchive.Parameter.SourceType.Material:                   return ShaderPatcherLayer.Node.Type.MaterialCBuffer;
                case ShaderFragmentArchive.Parameter.SourceType.InterpolatorIntoVertex:     return ShaderPatcherLayer.Node.Type.InterpolatorIntoVertex;
                case ShaderFragmentArchive.Parameter.SourceType.InterpolatorIntoPixel:      return ShaderPatcherLayer.Node.Type.InterpolatorIntoPixel;
                case ShaderFragmentArchive.Parameter.SourceType.System:                     return ShaderPatcherLayer.Node.Type.SystemCBuffer;
                case ShaderFragmentArchive.Parameter.SourceType.Output:                     return ShaderPatcherLayer.Node.Type.Output;
                case ShaderFragmentArchive.Parameter.SourceType.Constant:                   return ShaderPatcherLayer.Node.Type.Constants;
                default:                                                                    return ShaderPatcherLayer.Node.Type.Procedure;
            }
        }
            
        private static ShaderFragmentArchive.Parameter.SourceType AsSourceType(ShaderPatcherLayer.Node.Type input)
        {
            switch (input)
            {
                default:
                case ShaderPatcherLayer.Node.Type.MaterialCBuffer:          return ShaderFragmentArchive.Parameter.SourceType.Material;
                case ShaderPatcherLayer.Node.Type.InterpolatorIntoVertex:   return ShaderFragmentArchive.Parameter.SourceType.InterpolatorIntoVertex;
                case ShaderPatcherLayer.Node.Type.InterpolatorIntoPixel:    return ShaderFragmentArchive.Parameter.SourceType.InterpolatorIntoPixel;
                case ShaderPatcherLayer.Node.Type.SystemCBuffer:            return ShaderFragmentArchive.Parameter.SourceType.System;
                case ShaderPatcherLayer.Node.Type.Output:                   return ShaderFragmentArchive.Parameter.SourceType.Output;
                case ShaderPatcherLayer.Node.Type.Constants:                return ShaderFragmentArchive.Parameter.SourceType.Constant;
            }
        }

            //
            //      Convert from the "ViewModel" to the "Model"
            //
            //      We don't maintain the ShaderPatcherLayer.NodeGraph representation
            //      permanently... But we need this for serialization and shader
            //      generation operations
            //
            //      So, let's just build it from the graph control object.
            //
        public static ShaderPatcherLayer.NodeGraph ToShaderPatcherLayer(HyperGraph.GraphControl graphControl)
        {
            ShaderPatcherLayer.NodeGraph nodeGraph = new ShaderPatcherLayer.NodeGraph();
            Dictionary<Node, int> nodeToVisualNodeId = new Dictionary<Node, int>();
            foreach (Node n in graphControl.Nodes)
            {
                if (n.Tag is ShaderFragmentNodeTag)
                {
                    ShaderFragmentNodeTag nTag           = (ShaderFragmentNodeTag)n.Tag;
                    ShaderPatcherLayer.Node resultNode   = new ShaderPatcherLayer.Node();
                    resultNode.FragmentArchiveName       = nTag.ArchiveName;
                    resultNode.NodeId                    = nTag.Id;

                    if (n.Tag is ShaderParameterNodeTag)
                    {
                        //  This is a hack... But there should be a drop down list that
                        //  can tell us the type of this parameter struct box.
                        foreach (var i in n.Items)
                        {
                            if (i is HyperGraph.Items.NodeDropDownItem)
                            {
                                var dropDown = (HyperGraph.Items.NodeDropDownItem)i;
                                var stringForm = dropDown.Items[dropDown.SelectedIndex];
                                var parameterSource = ShaderFragmentNodeCreator.AsEnumValue<ShaderFragmentArchive.Parameter.SourceType>(stringForm, ShaderFragmentNodeCreator.ParamSourceTypeNames);
                                resultNode.NodeType = AsNodeType(parameterSource);
                            }
                        }
                    }
                    else
                    {
                        resultNode.NodeType = ShaderPatcherLayer.Node.Type.Procedure;
                    }

                    {
                        if (nodeToVisualNodeId.ContainsKey(n))
                        {
                            resultNode.VisualNodeId = nodeToVisualNodeId[n];
                        }
                        else
                        {
                            resultNode.VisualNodeId = nodeGraph.VisualNodes.Count();
                            nodeToVisualNodeId.Add(n, resultNode.VisualNodeId);
                            var visualNode = new ShaderPatcherLayer.VisualNode();
                            visualNode.Location = n.Location;
                            if (n.Collapsed) 
                            {
                                visualNode.State = ShaderPatcherLayer.VisualNode.StateType.Collapsed;
                            } 
                            else 
                            {
                                visualNode.State = ShaderPatcherLayer.VisualNode.StateType.Normal;
                            }
                            nodeGraph.VisualNodes.Add(visualNode);
                        }
                    }

                    nodeGraph.Nodes.Add(resultNode);

                    foreach (NodeConnection connection in n.Connections)
                    {
                        if (connection.From == null)
                        {
                                // this is a direct constant connection. It connects the value either to a constant value, or some named variable
                            if (connection.To != null)
                            {
                                Node destination = connection.To.Node;
                                if (destination.Tag is ShaderFragmentNodeTag)
                                {
                                    ShaderFragmentNodeTag dTag = (ShaderFragmentNodeTag)destination.Tag;
                                    ShaderPatcherLayer.NodeConstantConnection resultConnection = new ShaderPatcherLayer.NodeConstantConnection();
                                    resultConnection.Value = connection.Name;
                                    resultConnection.OutputNodeID = dTag.Id;
                                    if (connection.To.Item is ShaderFragmentNodeItem)
                                    {
                                        ShaderFragmentNodeItem destinationItem = (ShaderFragmentNodeItem)connection.To.Item;
                                        resultConnection.OutputParameterName = destinationItem.Name;
                                    }

                                    nodeGraph.NodeConstantConnections.Add(resultConnection);
                                }
                            }
                        }
                        else if (connection.From.Node == n)
                        {
                                // This is an output to the next node
                            Node destination = connection.To.Node;
                            if (destination.Tag is ShaderFragmentNodeTag)
                            {
                                ShaderFragmentNodeTag dTag = (ShaderFragmentNodeTag)destination.Tag;
                                ShaderPatcherLayer.NodeConnection resultConnection = new ShaderPatcherLayer.NodeConnection();
                                resultConnection.InputNodeID = nTag.Id;
                                resultConnection.OutputNodeID = dTag.Id;
                                if (connection.To.Item is ShaderFragmentNodeItem)
                                {
                                    ShaderFragmentNodeItem destinationItem = (ShaderFragmentNodeItem)connection.To.Item;
                                    resultConnection.OutputParameterName = destinationItem.Name;
                                    resultConnection.OutputType = TypeFromNodeItem(destinationItem);
                                }
                                if (connection.From.Item is ShaderFragmentNodeItem)
                                {
                                    ShaderFragmentNodeItem sourceItem = (ShaderFragmentNodeItem)connection.From.Item;
                                    resultConnection.InputParameterName = sourceItem.Name;
                                    resultConnection.InputType = TypeFromNodeItem(sourceItem);
                                }

                                nodeGraph.NodeConnections.Add(resultConnection);
                            }
                        }
                    }
                }
            }

            return nodeGraph;
        }

        private static string TypeFromNodeItem(ShaderFragmentNodeItem nodeItem)
        {
            if (nodeItem.ArchiveName != null && nodeItem.ArchiveName.Length != 0)
            {
                return ShaderFragmentArchive.Archive.GetParameter(nodeItem.ArchiveName).Type;
            }
            return nodeItem.Type;
        }

        public static void AddToHyperGraph(ShaderPatcherLayer.NodeGraph nodeGraph, HyperGraph.GraphControl graphControl, ShaderDiagram.Document doc)
        {
                //
                //      Convert from the "ShaderPatcherLayer" representation back to
                //      our graph control nodes. 
                //
                //      This is required for robust serialisation. We can't easily
                //      serialise in and out the graph control objects directly, because
                //      it risks problems if the rendering code or shader fragment objects
                //      change. That is, it's not very version-robust.
                //
                //      It's better to serialise a slightly higher level representation
                //      that just contains the node names and connections. That way, we 
                //      can adapt to changes in the tool and data.
                //
            {
                    // --------< Basic Nodes >--------
                var newNodes = new Dictionary<int, Node>();
                var nodeIdToControlNode = new Dictionary<UInt64, Node>();
                foreach (var n in nodeGraph.Nodes)
                {
                    if (!newNodes.ContainsKey(n.VisualNodeId))
                    {
                        if (n.NodeType == ShaderPatcherLayer.Node.Type.Procedure)
                        {
                            var fn = ShaderFragmentArchive.Archive.GetFunction(n.FragmentArchiveName);
                            if (fn != null)
                            {
                                var visualNode = nodeGraph.VisualNodes[n.VisualNodeId];
                                var newNode = ShaderFragmentNodeCreator.CreateNode(fn, n.FragmentArchiveName, graphControl, doc);
                                newNode.Location = visualNode.Location;
                                newNode.Collapsed = visualNode.State == ShaderPatcherLayer.VisualNode.StateType.Collapsed;
                                newNodes[n.VisualNodeId] = newNode;
                                nodeIdToControlNode[n.NodeId] = newNode;
                            }
                        }
                        else
                        {
                            var ps = ShaderFragmentArchive.Archive.GetParameterStruct(n.FragmentArchiveName);
                            if (ps != null)
                            {
                                var visualNode = nodeGraph.VisualNodes[n.VisualNodeId];
                                var newNode = ShaderFragmentNodeCreator.CreateParameterNode(ps, n.FragmentArchiveName, AsSourceType(n.NodeType));
                                newNode.Location = visualNode.Location;
                                newNode.Collapsed = visualNode.State == ShaderPatcherLayer.VisualNode.StateType.Collapsed;
                                newNodes[n.VisualNodeId] = newNode;
                                nodeIdToControlNode[n.NodeId] = newNode;
                            }
                        }
                    }
                }

                graphControl.AddNodes(newNodes.Values);

                    // --------< Node Connections >--------
                foreach (var c in nodeGraph.NodeConnections)
                {
                    if (nodeIdToControlNode.ContainsKey(c.InputNodeID) && nodeIdToControlNode.ContainsKey(c.OutputNodeID))
                    {
                        var inputItem = FindOrCreateNodeItem(nodeIdToControlNode[c.InputNodeID],
                            (item) => (item.Output != null && item.Output.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.InputParameterName)),
                            () => new ShaderFragmentNodeItem("result", c.InputType, null, false, true));
                        
                        var outputItem = FindOrCreateNodeItem(nodeIdToControlNode[c.OutputNodeID],
                            (item) => (item.Input != null && item.Input.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeItem(c.OutputParameterName, c.OutputType, null, true, false));

                        graphControl.Connect(inputItem.Output, outputItem.Input);
                    }
                }

                    // --------< Node Constant Connections >--------
                foreach (var c in nodeGraph.NodeConstantConnections)
                {
                    if (nodeIdToControlNode.ContainsKey(c.OutputNodeID))
                    {
                        var node = nodeIdToControlNode[c.OutputNodeID];
                        var outputItem = FindOrCreateNodeItem(node,
                            (item) => (item.Input != null && item.Input.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeItem(c.OutputParameterName, "float", null, true, false));

                        var connection = new NodeConnection();
                        connection.To = outputItem.Input;
                        connection.Name = c.Value;
                        node.AddConnection(connection);
                    }
                }

            }
        }

        private static NodeItem FindOrCreateNodeItem(Node node, Func<NodeItem, bool> predicate, Func<NodeItem> creator)
        {
            foreach (var i in node.Items)
            {
                if (predicate(i))
                {
                    return i;
                }
            }

            var newItem = creator();
            node.AddItem(newItem);
            return newItem;
        }
    }
}
