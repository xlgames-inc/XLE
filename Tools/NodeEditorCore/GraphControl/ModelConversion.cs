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
        ShaderPatcherLayer.NodeGraphFile ToShaderPatcherLayer(HyperGraph.IGraphModel graph);
        void AddToHyperGraph(ShaderPatcherLayer.NodeGraphFile graphFile, HyperGraph.IGraphModel graph);
    }

    [Export(typeof(IModelConversion))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ModelConversion : IModelConversion
    {
        private static ShaderPatcherLayer.Node.Type AsNodeType(ShaderFragmentArchive.Parameter.SourceType input)
        {
            switch (input)
            {
                case ShaderFragmentArchive.Parameter.SourceType.Material:                   return ShaderPatcherLayer.Node.Type.Captures;
                case ShaderFragmentArchive.Parameter.SourceType.System:                     return ShaderPatcherLayer.Node.Type.Captures;
                case ShaderFragmentArchive.Parameter.SourceType.Constant:                   return ShaderPatcherLayer.Node.Type.Captures;
                default:                                                                    return ShaderPatcherLayer.Node.Type.Procedure;
            }
        }
            
        private static ShaderFragmentArchive.Parameter.SourceType AsSourceType(ShaderPatcherLayer.Node.Type input)
        {
            switch (input)
            {
                default:
                case ShaderPatcherLayer.Node.Type.Procedure:            return ShaderFragmentArchive.Parameter.SourceType.Material;
                case ShaderPatcherLayer.Node.Type.Captures:             return ShaderFragmentArchive.Parameter.SourceType.Material;
            }
        }

        private static string UnplacedSubGraphTag = "unplaced";

        private ShaderPatcherLayer.NodeGraphSignature.Parameter AsSignatureParameter(ShaderFragmentInterfaceParameterItem item, ShaderPatcherLayer.NodeGraphSignature.ParameterDirection direction)
        {
            return new ShaderPatcherLayer.NodeGraphSignature.Parameter
            {
                Type = item.Type,
                Name = item.Name,
                Direction = direction,
                Semantic = item.Semantic,
                Default = item.Default
            };
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
        public ShaderPatcherLayer.NodeGraphFile ToShaderPatcherLayer(HyperGraph.IGraphModel graph)
        {
            ShaderPatcherLayer.NodeGraphFile result = new ShaderPatcherLayer.NodeGraphFile();
            foreach (Node n in graph.SubGraphs)
            {
                ShaderPatcherLayer.NodeGraphSignature signature = new ShaderPatcherLayer.NodeGraphSignature();
                // Note that OutputItems/InputItems are flipped for the subgraph nodes (because of how connectors interface with them)
                foreach (var item in n.OutputItems)
                {
                    var interfaceItem = item as ShaderFragmentInterfaceParameterItem;
                    if (interfaceItem != null)
                        signature.Parameters.Add(AsSignatureParameter(interfaceItem, ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In));
                }
                foreach (var item in n.InputItems)
                {
                    var interfaceItem = item as ShaderFragmentInterfaceParameterItem;
                    if (interfaceItem != null)
                        signature.Parameters.Add(AsSignatureParameter(interfaceItem, ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.Out));
                }

                signature.Implements = (n.TopItems.ElementAt(2) as HyperGraph.Items.NodeTextBoxItem).Text;

                result.SubGraphs.Add(
                    n.SubGraphTag as string,
                    new ShaderPatcherLayer.NodeGraphFile.SubGraph
                    {
                        Graph = new ShaderPatcherLayer.NodeGraph(),
                        Signature = signature
                    });
            }

            int visualNodeId = 0;
            Dictionary<Node, int> nodeToVisualNodeId = new Dictionary<Node, int>();
            foreach (Node n in graph.Nodes)
            {
                var nTag = n.Tag as ShaderFragmentNodeTag;
                if (nTag == null) continue;

                ShaderPatcherLayer.NodeGraphFile.SubGraph resultSubGraph = null;
                if ((n.SubGraphTag as string) != null)
                    result.SubGraphs.TryGetValue(n.SubGraphTag as string, out resultSubGraph);

                if (resultSubGraph == null)
                    result.SubGraphs.TryGetValue(UnplacedSubGraphTag, out resultSubGraph);

                if (resultSubGraph == null)
                {
                    resultSubGraph = new ShaderPatcherLayer.NodeGraphFile.SubGraph
                    {
                        Graph = new ShaderPatcherLayer.NodeGraph(),
                        Signature = new ShaderPatcherLayer.NodeGraphSignature()
                    };
                    result.SubGraphs.Add(UnplacedSubGraphTag, resultSubGraph);
                }

                {
                    string attributeTableName = "visualNode" + visualNodeId; ++visualNodeId;

                    ShaderPatcherLayer.Node resultNode = new ShaderPatcherLayer.Node() {
                        FragmentArchiveName = nTag.ArchiveName, NodeId = nTag.Id,
                        AttributeTableName = attributeTableName };

                    if (nTag is ShaderProcedureNodeTag)
                    {
                        resultNode.NodeType = ShaderPatcherLayer.Node.Type.Procedure;

                        var templatedName = System.Text.RegularExpressions.Regex.Match(resultNode.FragmentArchiveName, "(.*)<(.+)>");
                        if (templatedName.Success)
                        {
                            var templateParam = new ShaderPatcherLayer.NodeGraphSignature.TemplateParameter
                            {
                                Name = templatedName.Groups[1].Value,
                                Restriction = templatedName.Groups[2].Value
                            };
                            var existing = resultSubGraph.Signature.TemplateParameters.Where(x => String.Compare(x.Name, templateParam.Name) == 0).FirstOrDefault();
                            if (existing == null)
                            {
                                resultSubGraph.Signature.TemplateParameters.Add(templateParam);
                            }
                            else if (String.Compare(existing.Restriction, templateParam.Restriction) != 0)
                                throw new InvalidOperationException("Cannot create program because two template parameters with the same name disagree on restriction (parameter: " + templateParam.Name + " is restricted to both " + templateParam.Restriction + " and " + existing.Restriction);
                        }
                    }
                    else
                    {
                        System.Diagnostics.Debug.Assert(nTag is ShaderCapturesNodeTag);

                        resultNode.NodeType = ShaderPatcherLayer.Node.Type.Captures;

                        // for each output parameter on this node, we must ensure there is a capture parameter in the signature
                        foreach(var i in n.OutputItems)
                        {
                            var item = i as ShaderFragmentCaptureParameterItem;
                            if (item == null) continue;

                            var param = new ShaderPatcherLayer.NodeGraphSignature.Parameter
                            {
                                Name = nTag.ArchiveName + "." + item.Name,
                                Type = item.Type,
                                Direction = ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.Out,
                                Default = item.Default,
                                Semantic = item.Semantic
                            };

                            var existing = resultSubGraph.Signature.CapturedParameters.Where(x => String.Compare(x.Name, param.Name) == 0).FirstOrDefault();
                            if (existing != null)
                            {
                                if (String.Compare(existing.Type, param.Type) != 0)
                                    throw new InvalidOperationException("Cannot create program because two capture parameters with the same name disagree on type (parameter: " + param.Name + " as type " + param.Type + " and " + existing.Type);
                            }
                            else
                                resultSubGraph.Signature.CapturedParameters.Add(param);
                        }
                    }

                    resultSubGraph.Graph.Nodes.Add(resultNode);

                    var attributeTable = new Dictionary<string, string>();
                    attributeTable.Add("X", n.Location.X.ToString());
                    attributeTable.Add("Y", n.Location.Y.ToString());
                    attributeTable.Add("State", n.Collapsed ? "Collapsed" : "Normal");

                    // build the preview settings objects
                    foreach (var i in n.CenterItems)
                    {
                        var preview = i as ShaderFragmentPreviewItem;
                        if (preview == null) continue;

                        var settings = preview.PreviewSettings;
                        attributeTable.Add("PreviewGeometry", ShaderPatcherLayer.PreviewSettings.PreviewGeometryToString(preview.Geometry));
                        if (settings.OutputToVisualize != null)
                            attributeTable.Add("OutputToVisualize", settings.OutputToVisualize);
                    }

                    result.AttributeTables.Add(attributeTableName, attributeTable);
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
                    var dstNode = (connection.To != null) ? connection.To.Node.Tag as ShaderFragmentNodeTag : null;
                    var srcNode = (connection.From != null) ? connection.From.Node.Tag as ShaderFragmentNodeTag : null;

                    if (connection.From == null && dstNode != null)
                    {
                        // this is a direct constant connection. It connects the value either to a constant value, or some named variable
                        var match = System.Text.RegularExpressions.Regex.Match(connection.Text, "<(.*)>(?: if \\((.*)\\))?");
                        if (match.Success)
                        {
                            var parameterName = match.Groups[1].Value;
                            var existingParameter = resultSubGraph.Signature.Parameters.Where(x => string.Compare(x.Name, parameterName) == 0).FirstOrDefault();
                            if (existingParameter == null)
                            {
                                var parameterType = "auto";
                                var fn = _shaderFragments.GetFunction(dstNode.ArchiveName, result.GetSearchRules());
                                if (fn!=null)
                                {
                                    var param = fn.Signature.Parameters.Where(x => string.Compare(x.Name, ((ShaderFragmentNodeConnector)connection.To).Name) == 0).FirstOrDefault();
                                    if (param != null)
                                    {
                                        parameterType = param.Type;
                                    }
                                    else
                                    {
                                        var templ = fn.Signature.TemplateParameters.Where(x => string.Compare(x.Name, ((ShaderFragmentNodeConnector)connection.To).Name) == 0).FirstOrDefault();
                                        if (templ != null)
                                        {
                                            parameterType = "graph";
                                        }
                                    }
                                }
                                resultSubGraph.Signature.Parameters.Add(
                                    new ShaderPatcherLayer.NodeGraphSignature.Parameter
                                    {
                                        Type = parameterType,
                                        Name = parameterName,
                                        Direction = ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In
                                    });
                            }
                            string condition = "";
                            if (match.Groups.Count > 2) { condition = match.Groups[2].Value; }
                            resultSubGraph.Graph.Connections.Add(
                                new ShaderPatcherLayer.Connection
                                {
                                    OutputNodeID = dstNode.Id,
                                    OutputParameterName = ((ShaderFragmentNodeConnector)connection.To).Name,
                                    InputNodeID = ShaderPatcherLayer.Node.NodeId_Interface,
                                    InputParameterName = parameterName,
                                    Condition = condition
                                });
                        }
                        else
                        {
                            var paramName = connection.Text;
                            string condition = "";
                            match = System.Text.RegularExpressions.Regex.Match(connection.Text, "(.*)(?: if \\((.*)\\))?");
                            if (match.Groups.Count > 2)
                            {
                                paramName = match.Groups[1].Value;
                                condition = match.Groups[2].Value;
                            }
                            resultSubGraph.Graph.Connections.Add(
                                new ShaderPatcherLayer.Connection
                                {
                                    OutputNodeID = dstNode.Id,
                                    OutputParameterName = ((ShaderFragmentNodeConnector)connection.To).Name,
                                    InputNodeID = ShaderPatcherLayer.Node.NodeId_Constant,
                                    InputParameterName = paramName,
                                    Condition = condition
                                });
                        }
                    }
                    else if (srcNode != null && connection.To is ShaderFragmentInterfaceParameterItem)
                    {
                            // this is an output parameter.
                        var outputParam = (ShaderFragmentInterfaceParameterItem)connection.To;
                        resultSubGraph.Graph.Connections.Add(
                            new ShaderPatcherLayer.Connection
                                {
                                    OutputNodeID = ShaderPatcherLayer.Node.NodeId_Interface,
                                    OutputParameterName = outputParam.Name,
                                    InputNodeID = srcNode.Id,
                                    InputParameterName = ((ShaderFragmentNodeConnector)connection.From).Name,
                                    Condition = connection.Text
                            });
                    }
                    else if (dstNode != null && connection.From is ShaderFragmentInterfaceParameterItem)
                    {
                        //  it's an input parameter.
                        var inputParam = (ShaderFragmentInterfaceParameterItem)connection.From;
                        resultSubGraph.Graph.Connections.Add(
                            new ShaderPatcherLayer.Connection
                                {
                                    OutputNodeID = dstNode.Id,
                                    OutputParameterName = ((ShaderFragmentNodeConnector)connection.To).Name,
                                    InputNodeID = ShaderPatcherLayer.Node.NodeId_Interface,
                                    InputParameterName = inputParam.Name,
                                    Condition = connection.Text
                            });
                    }
                    else if (dstNode != null && srcNode != null)
                    {
                        resultSubGraph.Graph.Connections.Add(
                            new ShaderPatcherLayer.Connection()
                                {
                                    OutputNodeID = dstNode.Id,
                                    OutputParameterName = ((ShaderFragmentNodeConnector)connection.To).Name,
                                    InputNodeID = srcNode.Id,
                                    InputParameterName = ((ShaderFragmentNodeConnector)connection.From).Name,
                                    Condition = connection.Text
                                });
                    }
                }
            }

            return result;
        }

        private void MatchAttributeTable(HyperGraph.Node dst, Dictionary<string, string> attributeTable)
        {
            string x, y;
            if (attributeTable.TryGetValue("X", out x) && attributeTable.TryGetValue("Y", out y))
            {
                float fx, fy;
                if (Single.TryParse(x, out fx) && Single.TryParse(y, out fy))
                    dst.Location = new System.Drawing.PointF(fx, fy);
            }
            string stateType;
            if (attributeTable.TryGetValue("State", out stateType) && string.Compare(stateType, "Collapsed", true) == 0)
                dst.Collapsed = true;
        }

        static ShaderPatcherLayer.PreviewSettings MakePreviewSettingsFromAttributeTable(Dictionary<string, string> attributeTable)
        {
            var previewSettings = new ShaderPatcherLayer.PreviewSettings();
            if (attributeTable != null)
            {
                string value = null;
                if (attributeTable.TryGetValue("PreviewGeometry", out value))
                    previewSettings.Geometry = ShaderPatcherLayer.PreviewSettings.PreviewGeometryFromString(value);
                if (attributeTable.TryGetValue("OutputToVisualize", out value))
                    previewSettings.OutputToVisualize = value;
            }
            return previewSettings;
        }

        private ShaderPatcherLayer.NodeGraphSignature FindSignature(ShaderPatcherLayer.NodeGraphFile graphFile, String name)
        {
            ShaderPatcherLayer.NodeGraphFile.SubGraph sg;
            if (graphFile.SubGraphs.TryGetValue(name, out sg)) {
                return sg.Signature;
            }
            var fn = _shaderFragments.GetFunction(name, graphFile.GetSearchRules());
            return (fn != null) ? fn.Signature : null;
        }

        public void AddToHyperGraph(ShaderPatcherLayer.NodeGraphFile graphFile, HyperGraph.IGraphModel graph)
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
            foreach (var inputSubGraph in graphFile.SubGraphs)
            {
                    // --------< SubGraph >--------
                var subGraphTag = inputSubGraph.Key;
                Node subgraph = _nodeCreator.CreateSubGraph(subGraphTag, inputSubGraph.Value.Signature.Implements);
                subgraph.SubGraphTag = subGraphTag;
                graph.AddSubGraph(subgraph);

                foreach (var param in inputSubGraph.Value.Signature.Parameters)
                {
                    var item = new ShaderFragmentInterfaceParameterItem(param.Name, param.Type)
                    {
                        Semantic = param.Semantic,
                        Default = param.Default,
                        Direction = (param.Direction == ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In) ? InterfaceDirection.In : InterfaceDirection.Out
                    };
                    // Note that OutputItems/InputItems are flipped for the subgraph nodes (because of how connectors interface with them)
                    subgraph.AddItem(item, (param.Direction == ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In) ? Node.Dock.Output : Node.Dock.Input);
                }

                    // --------< Basic Nodes >--------
                var nodeIdToControlNode = new Dictionary<UInt64, Node>();
                var newNodes = new List<Node>();
                foreach (var n in inputSubGraph.Value.Graph.Nodes)
                {
                    if (!nodeIdToControlNode.ContainsKey(n.NodeId))
                    {
                        Dictionary<string, string> attributeTable = null;
                        if (n.AttributeTableName != null && n.AttributeTableName.Length > 0)
                            graphFile.AttributeTables.TryGetValue(n.AttributeTableName, out attributeTable);

                        Node newNode;

                        if (n.NodeType == ShaderPatcherLayer.Node.Type.Procedure)
                        {
                            var finalFnName = n.FragmentArchiveName;
                            var templatedName = System.Text.RegularExpressions.Regex.Match(n.FragmentArchiveName, "(.*)<(.+)>");
                            if (templatedName.Success)
                                finalFnName = templatedName.Groups[2].Value;

                            var sig = FindSignature(graphFile, finalFnName);
                            newNode = _nodeCreator.CreateNode(sig, n.FragmentArchiveName, MakePreviewSettingsFromAttributeTable(attributeTable));
                        }
                        else
                        {
                            System.Diagnostics.Debug.Assert(n.NodeType == ShaderPatcherLayer.Node.Type.Captures);

                            var captureGroupName = n.FragmentArchiveName;
                            var ps = new List<ShaderPatcherLayer.NodeGraphSignature.Parameter>();
                            foreach (var p in inputSubGraph.Value.Signature.CapturedParameters)
                            {
                                if (p.Direction != ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.Out) continue;

                                int firstDot = p.Name.IndexOf('.');
                                if (firstDot != -1 && string.Compare(p.Name.Substring(0, firstDot), captureGroupName) == 0)
                                {
                                    ps.Add(new ShaderPatcherLayer.NodeGraphSignature.Parameter
                                    {
                                        Type = p.Type,
                                        Name = p.Name.Substring(firstDot + 1),
                                        Direction = ShaderPatcherLayer.NodeGraphSignature.ParameterDirection.In,
                                        Default = p.Default,
                                        Semantic = p.Semantic
                                    });
                                }
                            }

                            newNode = _nodeCreator.CreateCapturesNode(captureGroupName, ps);
                        }

                        if (newNode != null)
                        {
                            if (attributeTable != null)
                                MatchAttributeTable(newNode, attributeTable);

                            newNode.SubGraphTag = subGraphTag;
                            nodeIdToControlNode[n.NodeId] = newNode;
                            newNodes.Add(newNode);
                        }
                    }
                }

                graph.AddNodes(newNodes);

                string defaultTypeName = "auto";

                // --------< Node Connections >--------
                foreach (var c in inputSubGraph.Value.Graph.Connections)
                {
                    var foundInput = nodeIdToControlNode.ContainsKey(c.InputNodeID);
                    var foundOutput = nodeIdToControlNode.ContainsKey(c.OutputNodeID);
                    if (foundInput && foundOutput)
                    {
                        var inputItem = FindOrCreateNodeItem(
                            nodeIdToControlNode[c.InputNodeID], Node.Dock.Output,
                            (item) => (item is ShaderFragmentNodeConnector && ((ShaderFragmentNodeConnector)item).Name.Equals(c.InputParameterName)),
                            () => new ShaderFragmentNodeConnector(c.InputParameterName, defaultTypeName));

                        var outputItem = FindOrCreateNodeItem(
                            nodeIdToControlNode[c.OutputNodeID], Node.Dock.Input,
                            (item) => (item is ShaderFragmentNodeConnector && ((ShaderFragmentNodeConnector)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeConnector(c.OutputParameterName, defaultTypeName));

                        graph.Connect(inputItem, outputItem, c.Condition);
                    }
                    else if (foundOutput && c.InputNodeID == ShaderPatcherLayer.Node.NodeId_Constant)
                    {
                            // --------< Constant Connection >--------
                        var node = nodeIdToControlNode[c.OutputNodeID];
                        var outputItem = FindOrCreateNodeItem(
                            node, Node.Dock.Input,
                            (item) => (item is ShaderFragmentNodeConnector && ((ShaderFragmentNodeConnector)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeConnector(c.OutputParameterName, defaultTypeName));

                        var connection = new NodeConnection();
                        connection.To = outputItem;
                        connection.Text = c.InputParameterName;
                        if (!string.IsNullOrEmpty(c.Condition))
                            connection.Text += " if (" + c.Condition + ")";
                        node.AddConnection(connection);
                    }
                    else if (foundOutput && c.InputNodeID == ShaderPatcherLayer.Node.NodeId_Interface)
                    {
                            // --------< Input Parameter Connection >--------
                        var dstItem = FindOrCreateNodeItem(
                            nodeIdToControlNode[c.OutputNodeID], Node.Dock.Input,
                            (item) => (item is ShaderFragmentNodeConnector && ((ShaderFragmentNodeConnector)item).Name.Equals(c.OutputParameterName)),
                            () => new ShaderFragmentNodeConnector(c.OutputParameterName, defaultTypeName));

                        // We can potentially compress this item to just a label on the connector.
                        const bool compressConnector = true;
                        if (compressConnector)
                        {
                            var connection = new NodeConnection();
                            connection.To = dstItem;
                            connection.Text = "<" + c.InputParameterName + ">";
                            if (!string.IsNullOrEmpty(c.Condition))
                                connection.Text += " if (" + c.Condition + ")";
                            nodeIdToControlNode[c.OutputNodeID].AddConnection(connection);
                        }
                        else
                        {
                            Node srcNode = subgraph;

                            var srcItem = FindOrCreateNodeItem(
                                srcNode, Node.Dock.Output,
                                (item) =>
                                {
                                    var i = item as ShaderFragmentInterfaceParameterItem;
                                    if (i == null) return false;
                                    return i.Name.Equals(c.InputParameterName);
                                },
                                () => new ShaderFragmentInterfaceParameterItem(c.InputParameterName, defaultTypeName) { Direction = InterfaceDirection.In });

                            graph.Connect(srcItem, dstItem, c.Condition);
                        }
                    }
                    else if (foundInput && c.OutputNodeID == ShaderPatcherLayer.Node.NodeId_Interface)
                    {
                            // --------< Output Parameter Connections >--------
                        var srcItem = FindOrCreateNodeItem(
                            nodeIdToControlNode[c.InputNodeID], Node.Dock.Output,
                            (item) => (item is ShaderFragmentNodeConnector && ((ShaderFragmentNodeConnector)item).Name.Equals(c.InputParameterName)),
                            () => new ShaderFragmentNodeConnector(c.InputParameterName, defaultTypeName));

                        Node dstNode = subgraph;

                        var dstItem = FindOrCreateNodeItem(
                            dstNode, Node.Dock.Input,
                            (item) =>
                            {
                                var i = item as ShaderFragmentInterfaceParameterItem;
                                if (i == null) return false;
                                return i.Name.Equals(c.OutputParameterName);
                            },
                            () => new ShaderFragmentInterfaceParameterItem(c.OutputParameterName, defaultTypeName) { Direction = InterfaceDirection.Out });

                        graph.Connect(srcItem, dstItem, c.Condition);
                    }
                }
            }
        }

        private static NodeConnector FindOrCreateNodeItem(Node node, Node.Dock dock, Func<NodeConnector, bool> predicate, Func<NodeConnector> creator)
        {
            foreach (var i in node.ItemsForDock(dock))
            {
                var c = i as NodeConnector;
                if (c != null && predicate(c))
                    return c;
            }

            var newItem = creator();
            node.AddItem(newItem, dock);
            return newItem;
        }

        [Import]
        ShaderFragmentArchive.Archive _shaderFragments;

        [Import]
        IShaderFragmentNodeCreator _nodeCreator;
    }
}
