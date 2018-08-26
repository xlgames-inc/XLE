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
        public IShaderFragmentNodeCreator NodeFactory { get; set; }

        public GraphControl()
        {
            // This is mostly just for the right-click menu now... We probably
            // no longer need to derive from HyperGraph.GraphControl -- instead, just turn this
            // into a helper class.
            // But, then again, maybe even HyperGraph.GraphControl could be split into several
            // small pieces using the ATF AdaptableControl type stuff...?
            _components = new System.ComponentModel.Container();
            _nodeMenu = CreateNodeMenu();
            _createSubGraphMenu = CreateSubGraphMenu();
            _emptySpaceMenu = CreateEmptySpaceMenu();
            ConnectorDoubleClick += OnConnectorDoubleClick;
            ShowElementMenu += OnShowElementMenu;
        }

        private ContextMenuStrip CreateNodeMenu()
        {
            var refreshToolStripMenuItem = new ToolStripMenuItem() { Text = "Refresh" };
            refreshToolStripMenuItem.Click += new EventHandler(this.RefreshToolStripMenuItem_Click);

            var setArchiveName = new ToolStripMenuItem() { Text = "Set Archive Name" };
            setArchiveName.Click += SetArchiveName_Click;

            return new ContextMenuStrip(this._components)
                {
                    Items = { refreshToolStripMenuItem, setArchiveName }
                };
        }

        public void AddContextMenuItem(string text, System.EventHandler handler)
        {
            var item = new ToolStripMenuItem() { Text = text };
            item.Click += handler;
            _nodeMenu.Items.Add(item);
        }

        private ContextMenuStrip CreateSubGraphMenu()
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

        private ContextMenuStrip CreateEmptySpaceMenu()
        {
            var item2 = new ToolStripMenuItem() { Text = "Create new sub-graph" };
            item2.Click += new EventHandler(this.OnCreateSubGraph);

            return new ContextMenuStrip(this._components)
            {
                Items = { item2 }
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
                    for (; ; )
                    {
                        var c = interfItem.Connectors.FirstOrDefault();
                        if (c == null) break;
                        GetGraphModel().Disconnect(c);
                    }
                    interfItem.Node.RemoveItem(interfItem);
                    GetGraphModel().InvokeMiscChange(true);
                }
            }
        }

        private bool IsInputConnector(NodeConnector connector)
        {
            return connector.Node.InputItems.Contains(connector);
        }

        private void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var interfItem = e.Connector as ShaderFragmentInterfaceParameterItem;
            if (interfItem != null)
            {
                EditInterfaceParameter(interfItem);
                return;
            }

            // For input connectors, we can try to add a constant connection
            if (IsInputConnector(e.Connector))
            {
                EditSimpleConnection(e.Connector);
            }
        }
        
        #region Context menu
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
            else
            if (e.Element is Node && ((Node)e.Element).Tag is ShaderSubGraphNodeTag)
            {
                _createSubGraphMenu.Tag = ((Node)e.Element).SubGraphTag;
                _createSubGraphMenu.Show(e.Position);
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
            else if (e.Element is NodeConnector && ((NodeConnector)e.Element) is ShaderFragmentNodeConnector)
            {
                NodeConnector conn = (NodeConnector)e.Element;
                var tag = (ShaderFragmentNodeConnector)conn;
                if (true)
                {
                    // pop up a context menu for this connector
                    var menu = new ContextMenuStrip();

                    var param = conn as ShaderFragmentInterfaceParameterItem;
                    if (param != null)
                    {
                        var editItem = new ToolStripMenuItem() { Text = "Edit parameter" };
                        editItem.Click += (object o, EventArgs a) => { EditInterfaceParameter(param); };
                        menu.Items.Add(editItem);
                    }

                    if (IsInputConnector(conn))
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

                    if (conn.Node.Connections.Where(x => x.To == conn || x.From == conn).Any())
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

        private object AttachedSubGraphTag(object senderObject)
        {
            if (senderObject is ToolStripMenuItem)
            {
                return ((ToolStripMenuItem)senderObject).GetCurrentParent().Tag;
            }
            return null;
        }

        private HyperGraph.Node GetNode(uint id)
        {
            return NodeFactory.FindNodeFromId(GetGraphModel(), id);
        }

        private HyperGraph.Node GetSubGraph(uint id)
        {
            var subgraphTag = NodeFactory.FindNodeFromId(GetGraphModel(), id).SubGraphTag;
            return GetGraphModel().SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
        }

        private ShaderFragmentPreviewItem GetPreviewItem(object sender)
        {
            var n = GetNode(AttachedId(sender));
            if (n == null) return null;
            return (ShaderFragmentPreviewItem)n.CenterItems.Where(x => x is ShaderFragmentPreviewItem).FirstOrDefault();
        }

        public class NodePreviewContext
        {
            public uint NodeId { get; set; }
            public ShaderPatcherLayer.PreviewSettings PreviewSettings { get; set; }
            public string SubGraph { get; set; }
        }

        public NodePreviewContext GetNodePreviewContext(object sender)
        {
            var previewItem = GetPreviewItem(sender);
            return new NodePreviewContext
            {
                NodeId = AttachedId(sender),
                PreviewSettings = previewItem.PreviewSettings,
                SubGraph = previewItem.Node.SubGraphTag as string
            };
        }

        private void RefreshToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var p = GetPreviewItem(sender);
            if (p!=null) p.InvalidateShaderStructure();
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
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = GetGraphModel().SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    subgraph.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, InterfaceDirection.In) { Semantic = fm.Semantic, Default = fm.Default }, Node.Dock.Output);
                }
            }
        }

        private void OnCreateOutputParameterNode(object sender, EventArgs e)
        {
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = GetGraphModel().SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            using (var fm = new InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    subgraph.AddItem(new ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type, InterfaceDirection.Out) { Semantic = fm.Semantic, Default = fm.Default }, Node.Dock.Input);
                }
            }
        }

        private bool HasSubGraphNamed(string name)
        {
            return GetGraphModel().SubGraphs.Where(x => string.Compare(x.SubGraphTag as string, name) == 0).Any();
        }

        private void OnCreateSubGraph(object sender, EventArgs e)
        {
            // pick out a unique name to seed the entry box
            string name = "main";
            if (HasSubGraphNamed(name))
            {
                for (int c=0; ; ++c)
                {
                    name = "subgraph" + c.ToString();
                    if (!HasSubGraphNamed(name)) break;
                }
            }

            using (var dialog = new HyperGraph.TextEditForm { InputText = name, Text = "New sub-graph name" })
            {
                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    if (!HasSubGraphNamed(dialog.InputText))
                    {
                        var n = NodeFactory.CreateSubGraph(dialog.InputText, string.Empty);
                        var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                        if (tag is System.Drawing.PointF)
                            n.Location = (System.Drawing.PointF)tag;
                        GetGraphModel().AddSubGraph(n);
                    }
                    else
                    {
                        MessageBox.Show("Cannot create sub-graph with name (" + dialog.InputText + ") because a sub-graph with that name already exists");
                    }
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
        private ContextMenuStrip _createSubGraphMenu;
        private ContextMenuStrip _emptySpaceMenu;
        private System.ComponentModel.Container _components;
    }
}
