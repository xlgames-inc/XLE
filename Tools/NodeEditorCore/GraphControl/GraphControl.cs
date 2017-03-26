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

            var setArchiveName = new ToolStripMenuItem() { Text = "Set Archive Name" };
            setArchiveName.Click += SetArchiveName_Click;

            return new ContextMenuStrip(this._components)
                {
                    Items = { showPreviewShaderItem, refreshToolStripMenuItem, setArchiveName }
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

        private string GetSimpleConnection(NodeConnector connector)
        {
            //  look for an existing simple connection attached to this connector
            return connector.Node.Connections.Where(x => x.To == connector && x.From == null).Select(x => x.Name).FirstOrDefault();
        }

        private void EditSimpleConnection(NodeConnector connector)
        {
            using (var dialog = new HyperGraph.TextEditForm())
            {
                var existing = GetSimpleConnection(connector);
                dialog.InputText = (!string.IsNullOrEmpty(existing)) ? existing : "1.0f";

                var result = dialog.ShowDialog();
                if (result == DialogResult.OK)
                {
                    bool foundExisting = false;
                    foreach (var i in connector.Node.Connections)
                        if (i.To == connector && i.From == null)
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
                        GetGraphModel().Connect(null, connector, dialog.InputText);

                    GetGraphModel().InvokeMiscChange(true);
                }
            }
        }

        private void DisconnectAll(NodeConnector connector)
        {
            var connectionsDupe = connector.Node.Connections.ToArray();
            foreach (var i in connectionsDupe)
                if (i.To == connector || i.From == connector)
                    GetGraphModel().Disconnect(i);
        }

        private void EditInterfaceParameter(ShaderFragmentInterfaceParameterItem interfItem)
        {
            using (var fm = new InterfaceParameterForm() { Name = interfItem.Name, Type = interfItem.Type, Semantic = interfItem.Semantic, Default = interfItem.Default })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    interfItem.Name = fm.Name;
                    interfItem.Type = fm.Type;
                    interfItem.Semantic = fm.Semantic;
                    interfItem.Default = fm.Default;
                    GetGraphModel().InvokeMiscChange(true);
                }
                else if (result == DialogResult.No)
                {
                    // we must disconnect before removing the item...
                    if (interfItem.Output != null)
                    {
                        for (; ; )
                        {
                            var c = interfItem.Output.Connectors.FirstOrDefault();
                            if (c == null) break;
                            GetGraphModel().Disconnect(c);
                        }
                    }
                    if (interfItem.Input != null)
                    {
                        for (; ; )
                        {
                            var c = interfItem.Input.Connectors.FirstOrDefault();
                            if (c == null) break;
                            GetGraphModel().Disconnect(c);
                        }
                    }
                    interfItem.Node.RemoveItem(interfItem);
                    GetGraphModel().InvokeMiscChange(true);
                }
            }
        }

        private void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var interfItem = e.Connector.Item as ShaderFragmentInterfaceParameterItem;
            if (interfItem != null)
            {
                EditInterfaceParameter(interfItem);
                return;
            }

            // For input connectors, we can try to add a constant connection
            if (e.Connector == e.Connector.Item.Input)
            {
                EditSimpleConnection(e.Connector);
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
            else if (e.Element is NodeConnector && ((NodeConnector)e.Element).Item is ShaderFragmentNodeItem)
            {
                NodeConnector conn = (NodeConnector)e.Element;
                var tag = (ShaderFragmentNodeItem)conn.Item;
                if (tag.ArchiveName != null)
                {
                    // pop up a context menu for this connector
                    var menu = new ContextMenuStrip();

                    var param = conn.Item as ShaderFragmentInterfaceParameterItem;
                    if (param != null)
                    {
                        var editItem = new ToolStripMenuItem() { Text = "Edit parameter" };
                        editItem.Click += (object o, EventArgs a) => { EditInterfaceParameter(param); };
                        menu.Items.Add(editItem);
                    }

                    if (conn == conn.Item.Input)
                    {
                        var existing = GetSimpleConnection(conn);
                        if (!string.IsNullOrEmpty(existing))
                        {
                            var editItem = new ToolStripMenuItem() { Text = "Edit simple connection" };
                            editItem.Click += (object o, EventArgs a) => { EditSimpleConnection(conn); };
                            menu.Items.Add(editItem);
                        }
                        else
                        {
                            var addItem = new ToolStripMenuItem() { Text = "Add simple connection" };
                            addItem.Click += (object o, EventArgs a) => { EditSimpleConnection(conn); };
                            menu.Items.Add(addItem);
                        }
                    }

                    if (conn.Node.Connections.Where(x=>x.To==conn||x.From==conn).Any())
                    {
                        var removeItem = new ToolStripMenuItem() { Text = "Disconnect" };
                        removeItem.Click += (object o, EventArgs a) => { DisconnectAll(conn); };
                        menu.Items.Add(removeItem);
                    }

                    if (menu.Items.Count > 0)
                    {
                        menu.Show(e.Position);
                        e.Cancel = false;
                    }
                }
            }
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

        private ShaderPatcherLayer.NodeGraph ConvertToShaderPatcherLayer(GUILayer.DirectorySearchRules searchRules)
        {
            return ModelConversion.ToShaderPatcherLayer(GetGraphModel(), searchRules);
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
                ConvertToShaderPatcherLayer(Document.SearchRules), AttachedId(sender), 
                p.PreviewSettings, (Document!=null) ? Document.GraphContext.Variables : null);

            ControlsLibrary.BasicControls.TextWindow.Show(
                System.Text.RegularExpressions.Regex.Replace(shader.Item1, @"\r\n|\n\r|\n|\r", "\r\n"));        // (make sure we to convert the line endings into windows form)
        }
        private void refreshToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var p = GetPreviewItem(sender);
            if (p!=null) p.InvalidateShaderStructure();
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
        private void SetArchiveName_Click(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n == null) return;

            var tag = n.Tag as ShaderFragmentNodeTag;
            if (tag == null) return;

            using (var dialog = new HyperGraph.TextEditForm { InputText = tag.ArchiveName })
            {
                    // careful -- should we validate that the archive name is at least reasonable?
                if (dialog.ShowDialog() == DialogResult.OK)
                    tag.ArchiveName = dialog.InputText;
            }
        }
        private void OnCreateInputParameterNode(object sender, EventArgs e)
        {
            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    var n = NodeFactory.CreateInterfaceNode("Inputs", InterfaceDirection.In);
                    var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                    if (tag is System.Drawing.PointF)
                        n.Location = (System.Drawing.PointF)tag;
                    n.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, InterfaceDirection.In) { Semantic = fm.Semantic, Default = fm.Default });

                    GetGraphModel().AddNode(n);
                }
            }
        }
        private void OnCreateOutputParameterNode(object sender, EventArgs e)
        {
            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    var n = NodeFactory.CreateInterfaceNode("Outputs", InterfaceDirection.Out);
                    var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                    if (tag is System.Drawing.PointF)
                        n.Location = (System.Drawing.PointF)tag;
                    n.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, InterfaceDirection.Out) { Semantic = fm.Semantic, Default = fm.Default });

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
