// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using HyperGraph;

namespace NodeEditorCore
{
    public static class GraphHelpers
    {
        public static void SetupDefaultHandlers(HyperGraph.IGraphModel model)
        {
            model.CompatibilityStrategy = new ShaderFragmentNodeCompatibility();
            model.ConnectionAdded += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionAdded);
            model.ConnectionAdding += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionAdding);
            model.ConnectionRemoving += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionRemoved);
            model.NodeAdded += new EventHandler<AcceptNodeEventArgs>(OnNodeAdded);
            model.NodeRemoved += new EventHandler<NodeEventArgs>(OnNodeRemoved);
            model.ConnectionRemoving += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionRemoved);
        }

        private static void OnNodeAdded(object sender, AcceptNodeEventArgs args) { OnNodesChange(); }
        private static void OnNodeRemoved(object sender, NodeEventArgs args) { OnNodesChange(); }
        private static void OnNodesChange() {}

        private static void OnConnectionAdding(object sender, AcceptNodeConnectionEventArgs e) {}
        private static void OnConnectionAdded(object sender, AcceptNodeConnectionEventArgs e) {}
        private static void OnConnectionRemoved(object sender, AcceptNodeConnectionEventArgs e) {}
    }

    public class GraphControl : HyperGraph.GraphControl, IDisposable
    {
        HyperGraph.IGraphModel  GetGraphModel() { return base._model; }
        public IModelConversion ModelConversion { get; set; }

        public GraphControl()
        {
            // This is mostly just for the right-click menu now... We probably
            // no longer need to derive from HyperGraph.GraphControl -- instead, just turn this
            // into a helper class.
            // But, then again, maybe even HyperGraph.GraphControl could be split into several
            // small pieces using the ATF AdaptableControl type stuff...?
            components = new System.ComponentModel.Container();
            nodeMenu = CreateNodeMenu();
            ConnectorDoubleClick += OnConnectorDoubleClick;
            ShowElementMenu += OnShowElementMenu;
        }

        private System.Windows.Forms.ContextMenuStrip CreateNodeMenu()
        {
            var result = new System.Windows.Forms.ContextMenuStrip(this.components);

            var showPreviewShaderItem = new System.Windows.Forms.ToolStripMenuItem();
            var refreshToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            var largePreviewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();

            showPreviewShaderItem.Name = "showPreviewShaderItem";
            showPreviewShaderItem.Size = new System.Drawing.Size(186, 22);
            showPreviewShaderItem.Text = "Show Preview Shader";
            showPreviewShaderItem.Click += new System.EventHandler(this.OnShowPreviewShader);

            refreshToolStripMenuItem.Name = "refreshToolStripMenuItem";
            refreshToolStripMenuItem.Size = new System.Drawing.Size(186, 22);
            refreshToolStripMenuItem.Text = "Refresh";
            refreshToolStripMenuItem.Click += new System.EventHandler(this.refreshToolStripMenuItem_Click);

            largePreviewToolStripMenuItem.Name = "largePreviewToolStripMenuItem";
            largePreviewToolStripMenuItem.Size = new System.Drawing.Size(186, 22);
            largePreviewToolStripMenuItem.Text = "Large Preview";
            largePreviewToolStripMenuItem.Click += new System.EventHandler(this.largePreviewToolStripMenuItem_Click);

            result.Items.AddRange(
                new System.Windows.Forms.ToolStripItem[] {
                    showPreviewShaderItem,
                    refreshToolStripMenuItem,
                    largePreviewToolStripMenuItem});
            result.Name = "NodeMenu";
            result.Size = new System.Drawing.Size(187, 92);
            return result;
        }

        private void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var dialog = new HyperGraph.TextEditForm();
            dialog.InputText = "1.0f";

            //  look for an existing simple connection attached to this connector
            foreach (var i in e.Node.Connections)
            {
                if (i.To == e.Connector && i.From == null)
                    dialog.InputText = i.Name;
            }

            var result = dialog.ShowDialog();
            if (result == DialogResult.OK)
            {
                bool foundExisting = false;
                foreach (var i in e.Node.Connections)
                    if (i.To == e.Connector && i.From == null)
                    {
                        // empty dialog text means we want to disconnect any existing connections
                        if (dialog.InputText.Length > 0) {
                            i.Name = dialog.InputText;
                        } else {
                            GetGraphModel().Disconnect(i);
                            break;
                        }
                        foundExisting = true;
                    }

                if (!foundExisting && dialog.InputText.Length > 0)
                    GetGraphModel().Connect(null, e.Connector, dialog.InputText);
            }
        }
        
        #region Element context menu
        void OnShowElementMenu(object sender, AcceptElementLocationEventArgs e)
        {
            if (e.Element is Node && ((Node)e.Element).Tag is ShaderProcedureNodeTag)
            {
                var tag = (ShaderProcedureNodeTag)((Node)e.Element).Tag;
                nodeMenu.Tag = tag.Id;
                nodeMenu.Show(e.Position);
                e.Cancel = false;
            }
            // if (e.Element is Node && ((Node)e.Element).Tag is ShaderParameterNodeTag)
            // {
            //     var tag = (ShaderParameterNodeTag)((Node)e.Element).Tag;
            //     parameterBoxMenu.Tag = tag.Id;
            //     parameterBoxMenu.Show(e.Position);
            //     e.Cancel = false;
            // }
            // else 
            // if (e.Element is ShaderFragmentNodeItem)
            // {
            //     var tag = (ShaderFragmentNodeItem)e.Element;
            //     if (tag.ArchiveName != null)
            //     {
            //         ShaderParameterUtil.EditParameter(GetGraphModel(), tag.ArchiveName);
            //         e.Cancel = false;
            //     }
            // }
            // else if (e.Element is NodeConnector && ((NodeConnector)e.Element).Item is ShaderFragmentNodeItem)
            // {
            //     var tag = (ShaderFragmentNodeItem)((NodeConnector)e.Element).Item;
            //     if (tag.ArchiveName != null)
            //     {
            //         ShaderParameterUtil.EditParameter(GetGraphModel(), tag.ArchiveName);
            //         e.Cancel = false;
            //     }
            // }
            else
            {
                // if you don't want to show a menu for this item (but perhaps show a menu for something more higher up) 
                // then you can cancel the event
                e.Cancel = true;
            }
        }

        private UInt32 AttachedId(object senderObject)
        {
            if (senderObject is ToolStripMenuItem)
            {
                var tag = ((ToolStripMenuItem)senderObject).GetCurrentParent().Tag;
                if (tag is UInt32)
                    return (UInt32)tag;
            }
            return 0;
        }

        private HyperGraph.Node GetNode(uint id)
        {
            return ShaderFragmentNodeUtil.GetShaderFragmentNode(GetGraphModel(), id);
        }

        private ShaderPatcherLayer.NodeGraph ConvertToShaderPatcherLayer()
        {
            return ModelConversion.ToShaderPatcherLayer(GetGraphModel());
        }

        private void OnShowPreviewShader(object sender, EventArgs e)
        {
            var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(ConvertToShaderPatcherLayer(), AttachedId(sender), "");
            MessageBox.Show(shader, "Generated shader", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
        }

        private void refreshToolStripMenuItem_Click(object sender, EventArgs e)
        {
            //      Find the attached preview items and invalidate
            var n = GetNode(AttachedId(sender));
            if (n != null)
            {
                foreach (NodeItem i in n.Items)
                {
                    if (i is ShaderFragmentPreviewItem)
                    {
                        ((ShaderFragmentPreviewItem)i).InvalidateShaderStructure();
                    }
                }
            }
        }

        private void largePreviewToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n != null)
            {
                var nodeId = ((ShaderFragmentNodeTag)n.Tag).Id;

                // generate a preview builder for this specific node...
                // var nodeGraph = ConvertToShaderPatcherLayer();
                // var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(nodeGraph, nodeId, "");
                // var builder = PreviewRender.Manager.Instance.CreatePreview(shader);

                // create a "LargePreview" window
                // new LargePreview(builder, _document).Show();
            }
        }

        private void addParameterToolStripMenuItem_Click(object sender, EventArgs e)
        {
            //      Add a new parameter to the attached parameter node
            // var n = ShaderFragmentNodeUtil.GetParameterNode(graphControl, AttachedId(sender));
            // if (n != null)
            // {
            //     var param = ShaderFragmentArchive.Archive.GetParameterStruct("LocalArchive[NewParameter]");
            //     if (param.Name.Length == 0)
            //     {
            //         param.Name = "NewParameter";
            //     }
            //     if (param.Type.Length == 0)
            //     {
            //         param.Type = "float";
            //     }
            // 
            //     n.AddItem(new ShaderFragmentNodeItem(param.Name, param.Type, param.ArchiveName, false, true));
            // }
        }
        
        #endregion

        public void Dispose()
        {
            if (components != null) { components.Dispose(); components = null; }
            if (nodeMenu != null) { nodeMenu.Dispose(); nodeMenu = null; }
        }

        private System.Windows.Forms.ContextMenuStrip nodeMenu;
        private System.ComponentModel.Container components;
    }
}
