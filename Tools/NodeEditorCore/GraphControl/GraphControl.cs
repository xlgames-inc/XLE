// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Windows.Forms;
using System.Linq;
using HyperGraph;

namespace NodeEditorCore
{
    public class GraphControl : HyperGraph.GraphControl, IDisposable
    {
        HyperGraph.IGraphModel  GetGraphModel() { return base._model; }
        public IModelConversion ModelConversion { get; set; }
        public IShaderFragmentNodeCreator NodeFactory { get; set; }
        public IDiagramDocument Document { get; set; }

        public GraphControl()
        {
            // This is mostly just for the right-click menu now... We probably
            // no longer need to derive from HyperGraph.GraphControl -- instead, just turn this
            // into a helper class.
            // But, then again, maybe even HyperGraph.GraphControl could be split into several
            // small pieces using the ATF AdaptableControl type stuff...?
            _components = new System.ComponentModel.Container();
            _nodeMenu = CreateNodeMenu();
            _emptySpaceMenu = CreateEmptySpaceMenu();
            ConnectorDoubleClick += OnConnectorDoubleClick;
            ShowElementMenu += OnShowElementMenu;
        }

        private ContextMenuStrip CreateNodeMenu()
        {
            var showPreviewShaderItem = new ToolStripMenuItem() { Text = "Show Preview Shader" };
            showPreviewShaderItem.Click += new EventHandler(this.OnShowPreviewShader);

            var refreshToolStripMenuItem = new ToolStripMenuItem() { Text = "Refresh" };
            refreshToolStripMenuItem.Click += new EventHandler(this.refreshToolStripMenuItem_Click);

            var largePreviewToolStripMenuItem = new ToolStripMenuItem() { Text = "Large Preview" };
            largePreviewToolStripMenuItem.Click += new EventHandler(this.largePreviewToolStripMenuItem_Click);

            return new ContextMenuStrip(this._components)
                {
                    Items = { showPreviewShaderItem, refreshToolStripMenuItem, largePreviewToolStripMenuItem }
                };
        }

        private ContextMenuStrip CreateEmptySpaceMenu()
        {
            var item0 = new ToolStripMenuItem() { Text = "Create Input" };
            item0.Click += new EventHandler(this.OnCreateInputParameterNode);

            var item1 = new ToolStripMenuItem() { Text = "Create Output" };
            item1.Click += new EventHandler(this.OnCreateOutputParameterNode);

            return new ContextMenuStrip(this._components)
            {
                Items = { item0, item1 }
            };
        }

        private void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var inputParam = e.Connector.Item as ShaderFragmentInterfaceParameterItem;
            if (inputParam != null)
            {
                using (var fm = new InterfaceParameterForm() { Name = inputParam.Name, Type = inputParam.Type, Semantic = inputParam.Semantic })
                {
                    var result = fm.ShowDialog();
                    if (result == DialogResult.OK)
                    {
                        inputParam.Name = fm.Name;
                        inputParam.Type = fm.Type;
                        inputParam.Semantic = fm.Semantic;
                        GetGraphModel().InvokeMiscChange(true);
                    }
                    else if (result == DialogResult.No)
                    {
                        // we must disconnect before removing the item...
                        if (inputParam.Output != null)
                        {
                            for (; ; )
                            {
                                var c = inputParam.Output.Connectors.FirstOrDefault();
                                if (c == null) break;
                                GetGraphModel().Disconnect(c);
                            }
                        }
                        if (inputParam.Input != null)
                        {
                            for (; ; )
                            {
                                var c = inputParam.Input.Connectors.FirstOrDefault();
                                if (c == null) break;
                                GetGraphModel().Disconnect(c);
                            }
                        }
                        inputParam.Node.RemoveItem(inputParam);
                        GetGraphModel().InvokeMiscChange(true);
                    }
                }
                return;
            }

            // For input connectors, we can try to add a constant connection
            if (e.Connector == e.Connector.Item.Input)
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
                            if (dialog.InputText.Length > 0)
                            {
                                i.Name = dialog.InputText;
                            }
                            else
                            {
                                GetGraphModel().Disconnect(i);
                                break;
                            }
                            foundExisting = true;
                        }

                    if (!foundExisting && dialog.InputText.Length > 0)
                        GetGraphModel().Connect(null, e.Connector, dialog.InputText);

                    GetGraphModel().InvokeMiscChange(true);
                }
            }
        }
        
        #region Element context menu
        void OnShowElementMenu(object sender, AcceptElementLocationEventArgs e)
        {
            if (e.Element == null)
            {
                _emptySpaceMenu.Tag = ClientToModel(new System.Drawing.PointF(e.Position.X, e.Position.Y));
                _emptySpaceMenu.Show(e.Position);
                e.Cancel = false;
            }
            else
            if (e.Element is Node && ((Node)e.Element).Tag is ShaderProcedureNodeTag)
            {
                var tag = (ShaderProcedureNodeTag)((Node)e.Element).Tag;
                _nodeMenu.Tag = tag.Id;
                _nodeMenu.Show(e.Position);
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
            return NodeFactory.FindNodeFromId(GetGraphModel(), id);
        }

        private ShaderPatcherLayer.NodeGraph ConvertToShaderPatcherLayer()
        {
            return ModelConversion.ToShaderPatcherLayer(GetGraphModel());
        }

        private ShaderFragmentPreviewItem GetPreviewItem(object sender)
        {
            var n = GetNode(AttachedId(sender));
            if (n == null) return null;
            return (ShaderFragmentPreviewItem)n.Items.Where(x => x is ShaderFragmentPreviewItem).FirstOrDefault();
        }

        private void OnShowPreviewShader(object sender, EventArgs e)
        {
            var p = GetPreviewItem(sender);
            var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(
                ConvertToShaderPatcherLayer(), AttachedId(sender), 
                p.PreviewSettings, (Document!=null) ? Document.GraphContext.Variables : null);
            var wnd = new ControlsLibrary.BasicControls.TextWindow();
            wnd.Text = System.Text.RegularExpressions.Regex.Replace(shader.Item1, @"\r\n|\n\r|\n|\r", "\r\n");        // (make sure we to convert the line endings into windows form)
            wnd.Show();
        }
        private void refreshToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var p = GetPreviewItem(sender);
            if (p!=null) p.InvalidateShaderStructure();
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
        private void OnCreateInputParameterNode(object sender, EventArgs e)
        {
            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "float4", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    var n = NodeFactory.CreateInterfaceNode("Inputs", InterfaceDirection.In);
                    var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                    if (tag is System.Drawing.PointF)
                        n.Location = (System.Drawing.PointF)tag;
                    n.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, fm.Semantic, InterfaceDirection.In));

                    GetGraphModel().AddNode(n);
                }
            }
        }
        private void OnCreateOutputParameterNode(object sender, EventArgs e)
        {
            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "float4", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    var n = NodeFactory.CreateInterfaceNode("Outputs", InterfaceDirection.Out);
                    var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                    if (tag is System.Drawing.PointF)
                        n.Location = (System.Drawing.PointF)tag;
                    n.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, fm.Semantic, InterfaceDirection.Out));

                    GetGraphModel().AddNode(n);
                }
            }
        }

        #endregion

        public void Dispose()
        {
            if (_components != null) { _components.Dispose(); _components = null; }
            if (_nodeMenu != null) { _nodeMenu.Dispose(); _nodeMenu = null; }
        }

        private ContextMenuStrip _nodeMenu;
        private ContextMenuStrip _emptySpaceMenu;
        private System.ComponentModel.Container _components;
    }
}
