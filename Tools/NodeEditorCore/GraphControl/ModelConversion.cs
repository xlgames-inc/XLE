// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text;
using HyperGraph;
using HyperGraph.Compatibility;
using HyperGraph.Items;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace NodeEditorCore
{
    public interface IModelConversion
    {
        ShaderPatcherLayer.NodeGraph ToShaderPatcherLayer(HyperGraph.IGraphModel graph);
        void AddToHyperGraph(ShaderPatcherLayer.NodeGraph nodeGraph, ShaderPatcherLayer.NodeGraphFile containingFile, HyperGraph.IGraphModel graph);
    }

    [Export(typeof(IModelConversion))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ModelConversion : IModelConversion
    {
        private static ShaderPatcherLayer.Node.Type AsNodeType(ShaderFragmentArchive.Parameter.SourceType input)
        {
            switch (input)
            {
                case ShaderFragmentArchive.Parameter.SourceType.Material:                   return ShaderPatcherLayer.Node.Type.Uniforms;
                case ShaderFragmentArchive.Parameter.SourceType.System:                     return ShaderPatcherLayer.Node.Type.Uniforms;
                case ShaderFragmentArchive.Parameter.SourceType.Output:                     return ShaderPatcherLayer.Node.Type.SlotOutput;
                case ShaderFragmentArchive.Parameter.SourceType.Constant:                   return ShaderPatcherLayer.Node.Type.Uniforms;
                default:                                                                    return ShaderPatcherLayer.Node.Type.Procedure;
            }
        }
            
        private static ShaderFragmentArchive.Parameter.SourceType AsSourceType(ShaderPatcherLayer.Node.Type input)
        {
            switch (input)
            {
                default:
                case ShaderPatcherLayer.Node.Type.Procedure:            return ShaderFragmentArchive.Parameter.SourceType.Material;
                case ShaderPatcherLayer.Node.Type.SlotInput:            return ShaderFragmentArchive.Parameter.SourceType.System;
                case ShaderPatcherLayer.Node.Type.SlotOutput:           return ShaderFragmentArchive.Parameter.SourceType.Output;
                case ShaderPatcherLayer.Node.Type.Uniforms:             return ShaderFragmentArchive.Parameter.SourceType.Material;
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
        public ShaderPatcherLayer.NodeGraph ToShaderPatcherLayer(HyperGraph.IGraphModel graph)
        {
            ShaderPatcherLayer.NodeGraph nodeGraph = new ShaderPatcherLayer.NodeGraph();
            Dictionary<Node, int> nodeToVisualNodeId = new Dictionary<Node, int>();
            foreach (Node n in graph.Nodes)
            {
                var nTag = n.Tag as ShaderFragmentNodeTag;
                if (nTag == null) continue;

                // Create a "visual node" (unless it already exists)
                //      --  this contains information that only affects the rendered
                //          graphs, not the output shader
                int visualNodeId;
                if (nodeToVisualNodeId.ContainsKey(n))
                {
                    visualNodeId = nodeToVisualNodeId[n];
                }
                else
                {
                    visualNodeId = nodeGraph.VisualNodes.Count();
                    nodeToVisualNodeId.Add(n, visualNodeId);
                    nodeGraph.VisualNodes.Add(
                        new ShaderPatcherLayer.VisualNode()
                        {
                            Location = n.Location,
                            State = (n.Collapsed) ? ShaderPatcherLayer.VisualNode.StateType.Collapsed : ShaderPatcherLayer.VisualNode.StateType.Normal
                        });
                }

                // Potentially build a "node" in the patcher layer. This is only required
                // if we're referencing some object in the shader fragments archive
                if (!string.IsNullOrEmpty(nTag.ArchiveName))
                {
                    ShaderPatcherLayer.Node resultNode = new ShaderPatcherLayer.Node() {
                        FragmentArchiveName = nTag.ArchiveName, NodeId = nTag.Id,
                        VisualNodeId = visualNodeId };

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

                    nodeGraph.Nodes.Add(resultNode);
                }

                // Build connections...
                foreach (NodeConnection connection in n.Connections)
                {
                    if (connection.To != null)
                    {
                        var dstNode = connection.To.Node.Tag as ShaderFragmentNodeTag;
                        if (dstNode == null) continue;

                        var dstItem = connection.To.Item as ShaderFragmentNodeItem;
                        if (dstItem == null) continue;

                        if (connection.From == null && connection.To.Node == n)
                        {
                                // this is a direct constant connection. It connects the value either to a constant value, or some named variable
                            nodeGraph.ConstantConnections.Add(
                                new ShaderPatcherLayer.ConstantConnection() 
                                    {   Value = connection.Name,
                                        OutputNodeID = dstNode.Id, OutputParameterName = dstItem.Name });
                        }
                        else if (connection.To.Node == n && connection.To.Item is ShaderFragmentInterfaceParameterItem)
                        {
                                // this is an output parameter. This is the only case where we creating the connection
                                // while processing the node on the "out" side
                            var outputParam = (ShaderFragmentInterfaceParameterItem)connection.To.Item;
                            var resultConnection = new ShaderPatcherLayer.OutputParameterConnection
                                {
                                    VisualNodeId = visualNodeId,    // (visual node for the output box)
                                    Type = outputParam.Type,
                                    Name = outputParam.Name,
                                    Semantic = outputParam.Semantic
                                };
                            if (connection.From.Item is ShaderFragmentNodeItem)
                            {
                                var tag = connection.From.Item.Node.Tag as ShaderFragmentNodeTag;
                                if (tag != null) resultConnection.InputNodeID = tag.Id;
                                resultConnection.InputParameterName = ((ShaderFragmentNodeItem)connection.From.Item).Name;
                            }
                            nodeGraph.OutputParameterConnections.Add(resultConnection);
                        }
                        else if (connection.From.Node == n)
                        {
                            if (connection.From.Item is ShaderFragmentInterfaceParameterItem)
                            {
                                // it's an input parameter... we just need type, name, semantic
                                var inputParam = (ShaderFragmentInterfaceParameterItem)connection.From.Item;
                                nodeGraph.InputParameterConnections.Add(
                                    new ShaderPatcherLayer.InputParameterConnection
                                        {
                                            OutputNodeID = dstNode.Id,
                                            OutputParameterName = dstItem.Name,
                                            VisualNodeId = visualNodeId,
                                            Type = inputParam.Type,
                                            Name = inputParam.Name,
                                            Semantic = inputParam.Semantic,
                                            Default = inputParam.Default
                                        });
                            }
                            else if (connection.To.Item is ShaderFragmentInterfaceParameterItem)
                            {
                                // it's an output parameter... 
                                // this will be handled when processing the node on the out side
                            }
                            else
                            {
                                // This is an output to the next node
                                var resultConnection = new ShaderPatcherLayer.NodeConnection()
                                    {
                                        InputNodeID = nTag.Id,
                                        OutputNodeID = dstNode.Id,
                                        OutputParameterName = dstItem.Name
                                    };

                                if (connection.From.Item is ShaderFragmentNodeItem)
                                {
                                    var sourceItem = (ShaderFragmentNodeItem)connection.From.Item;
                                    resultConnection.InputParameterName = sourceItem.Name;
                                    resultConnection.InputType = TypeFromNodeItem(sourceItem);
                                }
                                nodeGraph.NodeConnections.Add(resultConnection);
                            }
                        }
                    }
                    else if (connection.From != null && connection.From.Node == n)
                    {
                            // when connection.To is null, it could be an attached output connection
                        var resultConnection = new ShaderPatcherLayer.OutputParameterConnection()
                            {
                                Name = connection.Name,
                                InputNodeID = nTag.Id
                            };

                        if (connection.From.Item is ShaderFragmentNodeItem)
                        {
                            var sourceItem = (ShaderFragmentNodeItem)connection.From.Item;
                            resultConnection.InputParameterName = sourceItem.Name;
                            resultConnection.Type = TypeFromNodeItem(sourceItem);
                        }
                        nodeGraph.OutputParameterConnections.Add(resultConnection);
                    }
                }

                // build the preview settings objects
                foreach (var i in n.Items)
                {
                    var preview = i as ShaderFragmentPreviewItem;
                    if (preview == null) continue;
                    
                    var settings = preview.PreviewSettings;
                    settings.VisualNodeId = visualNodeId;
                    nodeGraph.PreviewSettingsObjects.Add(settings);
                }
            }

            return nodeGraph;
        }

        private string TypeFromNodeItem(ShaderFragmentNodeItem nodeItem)
        {
            if (nodeItem.ArchiveName != null && nodeItem.ArchiveName.Length != 0)
            {
                var param = _shaderFragments.GetParameter(nodeItem.ArchiveName, null);
                return (param != null) ? param.Type : string.Empty;
            }
            return nodeItem.Type;
        }

        private void MatchVisualNode(HyperGraph.Node dst, ShaderPatcherLayer.VisualNode src)
        {
            dst.Location = src.Location;
            dst.Collapsed = src.State == ShaderPatcherLayer.VisualNode.StateType.Collapsed;
        }

        public void AddToHyperGraph(ShaderPatcherLayer.NodeGraph nodeGraph, ShaderPatcherLayer.NodeGraphFile containingFile, HyperGraph.IGraphModel graph)
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
                        var visualNode = nodeGraph.VisualNodes[n.VisualNodeId];

                        // also look for preview settings...
                        var previewSettings = nodeGraph.PreviewSettingsObjects.Where(x => x.VisualNodeId == n.VisualNodeId).FirstOrDefault();
                        Node newNode = null;
                        if (n.NodeType == ShaderPatcherLayer.Node.Type.Procedure)
                        {
                            var fn = containingFile.FindSignature(n.FragmentArchiveName);
                            newNode = _nodeCreator.CreateNode(fn, n.FragmentArchiveName, previewSettings);
                        }
                        else
                        {
                            System.Diagnostics.Debug.Assert(false);
                            // var ps = _shaderFragments.GetParameterStruct(n.FragmentArchiveName, nodeGraph.SearchRules);
                            // newNode = _nodeCreator.CreateParameterNode(ps, n.FragmentArchiveName, AsSourceType(n.NodeType));
                        }

                        if (newNode != null)
                        {
                            MatchVisualNode(newNode, visualNode);
                            newNodes[n.VisualNodeId] = newNode;
                            nodeIdToControlNode[n.NodeId] = newNode;
                        }
                    }
                }

                graph.AddNodes(newNodes.Values);

                    // --------< Node Connections >--------
                foreach (var c in nodeGraph.NodeConnections)
                {
                    if (nodeIdToControlNode.ContainsKey(c.InputNodeID) && nodeIdToControlNode.ContainsKey(c.OutputNodeID))
                    {
                        var inputItem = FindOrCreateNodeItem(nodeIdToControlNode[c.InputNodeID],
                            (item) => (item.Output != null && item.Output.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.InputParameterName)),
                            () => new ShaderFragmentNodeItem(c.InputParameterName, c.InputType, null, false, true));
                        
                        var outputItem = FindOrCreateNodeItem(nodeIdToControlNode[c.OutputNodeID],
                            (item) => (item.Input != null && item.Input.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeItem(c.OutputParameterName, "", null, true, false));

                        graph.Connect(inputItem.Output, outputItem.Input);
                    }
                }

                    // --------< Constant Connections >--------
                foreach (var c in nodeGraph.ConstantConnections)
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

                    // --------< Input Parameter Connections >--------
                foreach (var c in nodeGraph.InputParameterConnections)
                {
                    if (!nodeIdToControlNode.ContainsKey(c.OutputNodeID)) continue;

                    var dstItem = FindOrCreateNodeItem(
                        nodeIdToControlNode[c.OutputNodeID],
                        (item) => (item.Input != null && item.Input.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.OutputParameterName)),
                        () => new ShaderFragmentNodeItem(c.OutputParameterName, c.Type, null, true, false));

                    Node srcNode;
                    if (!newNodes.ContainsKey(c.VisualNodeId))
                    {
                        srcNode = _nodeCreator.CreateInterfaceNode("Inputs", InterfaceDirection.In);
                        MatchVisualNode(srcNode, nodeGraph.VisualNodes[c.VisualNodeId]);
                        newNodes[c.VisualNodeId] = srcNode;
                        graph.AddNode(srcNode);
                    } 
                    else
                        srcNode = newNodes[c.VisualNodeId];

                    var srcItem = FindOrCreateNodeItem(
                        srcNode,
                        (item) => {
                            var i = item as ShaderFragmentInterfaceParameterItem;
                            if (i==null) return false;
                            return i.Name.Equals(c.Name) && i.Type.Equals(c.Type) && i.Semantic.Equals(c.Semantic);
                        },
                        () => new ShaderFragmentInterfaceParameterItem(c.Name, c.Type, InterfaceDirection.In) { Semantic = c.Semantic, Default = c.Default });

                    graph.Connect(srcItem.Output, dstItem.Input);
                }

                    // --------< Output Parameter Connections >--------
                foreach (var c in nodeGraph.OutputParameterConnections)
                {
                    if (!nodeIdToControlNode.ContainsKey(c.InputNodeID)) continue;

                    var srcItem = FindOrCreateNodeItem(
                        nodeIdToControlNode[c.InputNodeID],
                        (item) => (item.Output != null && item.Output.Enabled && item is ShaderFragmentNodeItem && ((ShaderFragmentNodeItem)item).Name.Equals(c.InputParameterName)),
                        () => new ShaderFragmentNodeItem(c.InputParameterName, c.Type, null, false, true));

                    Node dstNode;
                    if (!newNodes.ContainsKey(c.VisualNodeId))
                    {
                        dstNode = _nodeCreator.CreateInterfaceNode("Outputs", InterfaceDirection.Out);
                        MatchVisualNode(dstNode, nodeGraph.VisualNodes[c.VisualNodeId]);
                        newNodes[c.VisualNodeId] = dstNode;
                        graph.AddNode(dstNode);
                    }
                    else
                        dstNode = newNodes[c.VisualNodeId];

                    var dstItem = FindOrCreateNodeItem(
                        dstNode,
                        (item) =>
                        {
                            var i = item as ShaderFragmentInterfaceParameterItem;
                            if (i == null) return false;
                            return i.Name.Equals(c.Name) && i.Type.Equals(c.Type) && i.Semantic.Equals(c.Semantic);
                        },
                        () => new ShaderFragmentInterfaceParameterItem(c.Name, c.Type, InterfaceDirection.Out) { Semantic = c.Semantic });

                    graph.Connect(srcItem.Output, dstItem.Input);
                }
            }
        }

        private static NodeItem FindOrCreateNodeItem(Node node, Func<NodeItem, bool> predicate, Func<NodeItem> creator)
        {
            foreach (var i in node.Items)
                if (predicate(i))
                    return i;

            var newItem = creator();
            node.AddItem(newItem);
            return newItem;
        }

        [Import]
        ShaderFragmentArchive.Archive _shaderFragments;

        [Import]
        IShaderFragmentNodeCreator _nodeCreator;
    }
}
