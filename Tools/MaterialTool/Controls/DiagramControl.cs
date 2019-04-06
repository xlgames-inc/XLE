// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf.Controls;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Applications;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Collections.Generic;
using System.Linq;
using System;
using Sce.Atf;
using System.Windows.Forms;
using Sce.Atf.Adaptation;
using System.Drawing.Drawing2D;
using HyperGraphAdapter;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace MaterialTool.Controls
{
    interface IGraphControl
    {
        void SetContext(DiagramEditingContext context);
    }

    [Export(typeof(IGraphControl))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    class SubGraphControl : AdaptableControl, IGraphControl
    {
        public SubGraphControl()
        {
            this.AllowDrop = true;
            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(70)))), ((int)(((byte)(70)))), ((int)(((byte)(70)))));
            Paint += child_Paint;

            _components = new System.ComponentModel.Container();

            _createSubGraphMenu = CreateSubGraphMenu();
            _emptySpaceMenu = CreateEmptySpaceMenu();
        }

        public new void Dispose()
        {
            if (_components != null) { _components.Dispose(); _components = null; }
            base.Dispose();
        }

        public void SetContext(DiagramEditingContext context)
        {
            var existingContext = ContextAs<ISelectionContext>();
            if (existingContext != null)
                existingContext.SelectionChanged -= Context_SelectionChanged;

            var graphAdapter = new HyperGraphAdapter.HyperGraphAdapter();
            graphAdapter.HighlightCompatible = true;
            graphAdapter.LargeGridStep = 160F;
            graphAdapter.SmallGridStep = 20F;
            graphAdapter.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(90)))), ((int)(((byte)(90)))), ((int)(((byte)(90)))));
            graphAdapter.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(80)))), ((int)(((byte)(80)))), ((int)(((byte)(80)))));
            graphAdapter.ShowLabels = true;
            graphAdapter.Model = context.ViewModel;
            graphAdapter.Selection = context.DiagramSelection; 
            graphAdapter.Context = context;

            graphAdapter.ConnectorDoubleClick += OnConnectorDoubleClick;
            graphAdapter.ShowElementMenu += OnShowElementMenu;

            this.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(50)))), ((int)(((byte)(50)))), ((int)(((byte)(50)))));
            graphAdapter.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(45)))), ((int)(((byte)(45)))), ((int)(((byte)(45)))));
            graphAdapter.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(55)))), ((int)(((byte)(55)))), ((int)(((byte)(55)))));

            // calling Adapt will unbind previous adapters
            var hoverAdapter = new HoverAdapter();
            hoverAdapter.HoverStarted += (object sender, HoverEventArgs<object, object> args) =>
                {
                    if (_hover != null) return;

                    var n = args.Object as HyperGraph.Node;
                    if (n == null)
                    {
                        var i = args.Object as HyperGraph.NodeItem;
                        if (i != null) n = i.Node;
                    }
                    if (n != null && n.Title != null && n.Title.Length > 0)
                    {
                        _hover = new HoverLabel(n.Title)
                        {
                            Location = new Point(MousePosition.X - 8, MousePosition.Y + 8)
                        };
                        _hover.ShowWithoutFocus();
                    }
                };
            hoverAdapter.HoverStopped += EndHover;
            MouseLeave += EndHover;
            Adapt(
                new IControlAdapter[] {
                    graphAdapter,
                    new PickingAdapter(),
                    new CanvasAdapter(),
                    new ViewingAdapter(graphAdapter),
                    hoverAdapter
                });
            context.SelectionChanged += Context_SelectionChanged;

            // Our context is actually a collection of 2 separate context objects
            // - What represents the model itself
            // - Another is the "ViewingContext", which is how we're looking at the model
            // Curiously, there seems to be a bit of an overlap between control adapters
            // and the viewing context. For example, ViewingContext, PickingAdapter and ViewingAdapter
            // duplicate some of the same functionality.
            // However, all of these are needed to use the standard ATF commands for framing and aligning
            _contextSet = new AdaptableSet(new object[]{ context, new ViewingContext { Control = this } });
            Context = _contextSet;
        }

        private void Context_SelectionChanged(object sender, EventArgs e)
        {
            Invalidate();
        }

        private void OnShowPreviewShader(object sender, EventArgs e)
        {
            var p = GetNodePreviewContext(sender);
            var graphContext = ContextAs<DiagramEditingContext>();
            var shader = GUILayer.ShaderGeneratorLayer.GeneratePreviewShader(
                graphContext.Document.NodeGraphFile,
                p.SubGraph, p.NodeId, p.PreviewSettings,
                graphContext.Document.GraphMetaData.Variables);

            ControlsLibrary.BasicControls.TextWindow.Show(
                System.Text.RegularExpressions.Regex.Replace(shader, @"\r\n|\n\r|\n|\r", System.Environment.NewLine));        // (make sure we to convert the line endings into windows form)
        }

        private void EndHover(object sender, EventArgs args)
        {
            if (_hover == null) return;
            _hover.Hide();
            _hover.Dispose();
            _hover = null;
        }

        void child_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
        {
                // update the "defaults material"
            var context = ContextAs<DiagramEditingContext>();
            if (context.Document.GraphMetaData.DefaultsMaterial != _activeMaterialContext.MaterialName) {
                context.Document.GraphMetaData.DefaultsMaterial = _activeMaterialContext.MaterialName;
            }
            GUILayer.EngineDevice.GetInstance().ForegroundUpdate();
        }

        private HyperGraph.IGraphModel ViewModel { get { return ContextAs<DiagramEditingContext>().ViewModel; } }
        private NodeEditorCore.IDiagramDocument Document { get { return ContextAs<DiagramEditingContext>().Document; } }

        #region Behaviour

        private ContextMenuStrip CreateNodeMenu(NodeEditorCore.ProcedureNodeType type)
        {
            var refreshToolStripMenuItem = new ToolStripMenuItem() { Text = "Refresh" };
            refreshToolStripMenuItem.Click += new EventHandler(this.RefreshToolStripMenuItem_Click);

            var setArchiveName = new ToolStripMenuItem() { Text = "Set Archive Name" };
            setArchiveName.Click += SetArchiveName_Click;

            ContextMenuStrip result = new ContextMenuStrip(this._components)
            {
                Items = { refreshToolStripMenuItem, setArchiveName }
            };

            if (type != NodeEditorCore.ProcedureNodeType.Instantiation)
            {
                var convert = new ToolStripMenuItem() { Text = "Convert to Instantiation Node" };
                convert.Click += new EventHandler(this.OnConvertToInstantiationNode);
                result.Items.Add(convert);
            }

            if (type != NodeEditorCore.ProcedureNodeType.TemplateParameter)
            {
                var convert = new ToolStripMenuItem() { Text = "Convert to Template Parameter" };
                convert.Click += new EventHandler(this.OnConvertToTemplateParameter);
                result.Items.Add(convert);
            }

            if (type != NodeEditorCore.ProcedureNodeType.Normal)
            {
                var convert = new ToolStripMenuItem() { Text = "Convert to Normal Node" };
                convert.Click += new EventHandler(this.OnConvertToNormalNode);
                result.Items.Add(convert);
            }

            var showPreviewShader = new ToolStripMenuItem() { Text = "Show Preview Shader" };
            showPreviewShader.Click += OnShowPreviewShader;
            result.Items.Add(showPreviewShader);

            return result;
        }

        private ContextMenuStrip CreateSubGraphMenu()
        {
            var item0 = new ToolStripMenuItem() { Text = "Set Sub-Graph Properties" };
            item0.Click += new EventHandler(this.OnSetSubGraphProperties);

            var item1 = new ToolStripMenuItem() { Text = "Create Input" };
            item1.Click += new EventHandler(this.OnCreateInputParameterNode);

            var item2 = new ToolStripMenuItem() { Text = "Create Output" };
            item2.Click += new EventHandler(this.OnCreateOutputParameterNode);

            var item3 = new ToolStripMenuItem() { Text = "Create Captures Group" };
            item3.Click += new EventHandler(this.OnCreateCapturesGroup);

            var item4 = new ToolStripMenuItem() { Text = "Delete Sub-Graph" };
            item4.Click += new EventHandler(this.OnDeleteSubGraph);

            return new ContextMenuStrip(this._components)
            {
                Items = { item0, item1, item2, item3, item4 }
            };
        }

        private ContextMenuStrip CreateEmptySpaceMenu()
        {
            var item2 = new ToolStripMenuItem() { Text = "Create New Sub-Graph" };
            item2.Click += new EventHandler(this.OnCreateSubGraph);

            return new ContextMenuStrip(this._components)
            {
                Items = { item2 }
            };
        }

        private string GetSimpleConnection(HyperGraph.NodeConnector connector)
        {
            //  look for an existing simple connection attached to this connector
            return connector.Node.Connections.Where(x => x.To == connector && x.From == null).Select(x => x.Text).FirstOrDefault();
        }

        private void EditSimpleConnection(HyperGraph.NodeConnector connector)
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
                                i.Text = dialog.InputText;
                            }
                            else
                            {
                                ViewModel.Disconnect(i);
                                break;
                            }
                            foundExisting = true;
                        }

                    if (!foundExisting && dialog.InputText.Length > 0)
                        ViewModel.Connect(null, connector, dialog.InputText);

                    ViewModel.InvokeMiscChange(true);
                }
            }
        }

        private void DisconnectAll(HyperGraph.NodeConnector connector)
        {
            var connectionsDupe = connector.Node.Connections.ToArray();
            foreach (var i in connectionsDupe)
                if (i.To == connector || i.From == connector)
                    ViewModel.Disconnect(i);
        }

        private void EditInterfaceParameter(NodeEditorCore.ShaderFragmentAdaptableParameterConnector interfItem)
        {
            using (var fm = new NodeEditorCore.InterfaceParameterForm() { Name = interfItem.Name, Type = interfItem.Type, Semantic = interfItem.Semantic, Default = interfItem.Default })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    interfItem.Name = fm.Name;
                    interfItem.Type = fm.Type;
                    interfItem.Semantic = fm.Semantic;
                    interfItem.Default = fm.Default;
                    ViewModel.InvokeMiscChange(true);
                }
                else if (result == DialogResult.No)
                {
                    // we must disconnect before removing the item...
                    for (; ; )
                    {
                        var c = interfItem.Connectors.FirstOrDefault();
                        if (c == null) break;
                        ViewModel.Disconnect(c);
                    }
                    interfItem.Node.RemoveItem(interfItem);
                    ViewModel.InvokeMiscChange(true);
                }
            }
        }

        private void EditConnectionCondition(HyperGraph.NodeConnection connection)
        {
            using (var fm = new HyperGraph.TextEditForm() { InputText = connection.Text })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    connection.Text = fm.InputText;
                }
            }
        }

        private bool IsInputConnector(HyperGraph.NodeConnector connector)
        {
            return connector.Node.InputItems.Contains(connector);
        }

        private void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var interfItem = e.Connector as NodeEditorCore.ShaderFragmentAdaptableParameterConnector;
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
        void OnShowElementMenu(object sender, HyperGraph.AcceptElementLocationEventArgs e)
        {
            var graphControl = sender as HyperGraph.GraphControl;
            if (e.Element == null)
            {
                _emptySpaceMenu.Tag = graphControl.ClientToModel(new System.Drawing.PointF(e.Position.X, e.Position.Y));
                _emptySpaceMenu.Show(e.Position);
                e.Cancel = false;
            }
            else
            if (e.Element is HyperGraph.Node && ((HyperGraph.Node)e.Element).Tag is NodeEditorCore.ShaderProcedureNodeTag)
            {
                var tag = (NodeEditorCore.ShaderProcedureNodeTag)((HyperGraph.Node)e.Element).Tag;
                var menu = CreateNodeMenu(tag.Type);
                menu.Tag = tag.Id;
                menu.Show(e.Position);
                e.Cancel = false;
            }
            else
            if (e.Element is HyperGraph.Node && ((HyperGraph.Node)e.Element).Tag is NodeEditorCore.ShaderSubGraphNodeTag)
            {
                _createSubGraphMenu.Tag = ((HyperGraph.Node)e.Element).SubGraphTag;
                _createSubGraphMenu.Show(e.Position);
                e.Cancel = false;
            }
            else if (e.Element is HyperGraph.NodeConnector && ((HyperGraph.NodeConnector)e.Element) is NodeEditorCore.ShaderFragmentNodeConnector)
            {
                HyperGraph.NodeConnector conn = (HyperGraph.NodeConnector)e.Element;
                var tag = (NodeEditorCore.ShaderFragmentNodeConnector)conn;
                if (true)
                {
                    // pop up a context menu for this connector
                    var menu = new ContextMenuStrip();

                    var param = conn as NodeEditorCore.ShaderFragmentAdaptableParameterConnector;
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
            else if (e.Element is HyperGraph.NodeConnection)
            {
                var menu = new ContextMenuStrip();

                var editItem = new ToolStripMenuItem() { Text = "Set Condition" };
                editItem.Click += (object o, EventArgs a) => { EditConnectionCondition(e.Element as HyperGraph.NodeConnection); };
                menu.Items.Add(editItem);

                menu.Show(e.Position);
                e.Cancel = false;
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
            return _nodeFactory.FindNodeFromId(ViewModel, id);
        }

        private HyperGraph.Node GetSubGraph(uint id)
        {
            var subgraphTag = _nodeFactory.FindNodeFromId(ViewModel, id).SubGraphTag;
            return ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
        }

        private NodeEditorCore.ShaderFragmentPreviewItem GetPreviewItem(object sender)
        {
            var n = GetNode(AttachedId(sender));
            if (n == null) return null;
            return (NodeEditorCore.ShaderFragmentPreviewItem)n.CenterItems.Where(x => x is NodeEditorCore.ShaderFragmentPreviewItem).FirstOrDefault();
        }

        public class NodePreviewContext
        {
            public uint NodeId { get; set; }
            public GUILayer.PreviewSettings PreviewSettings { get; set; }
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
            var n = GetNode(AttachedId(sender));
            if (n != null) _nodeFactory.UpdateProcedureNode(Document.NodeGraphFile, n);

            var p = GetPreviewItem(sender);
            if (p != null) p.InvalidateShaderStructure();
        }

        private void OnConvertToInstantiationNode(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n != null)
                _nodeFactory.SetProcedureNodeType(Document.NodeGraphFile, n, NodeEditorCore.ProcedureNodeType.Instantiation);
        }

        private void OnConvertToTemplateParameter(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n != null)
                _nodeFactory.SetProcedureNodeType(Document.NodeGraphFile, n, NodeEditorCore.ProcedureNodeType.TemplateParameter);
        }

        private void OnConvertToNormalNode(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n != null)
                _nodeFactory.SetProcedureNodeType(Document.NodeGraphFile, n, NodeEditorCore.ProcedureNodeType.Normal);
        }

        private void SetArchiveName_Click(object sender, EventArgs e)
        {
            var n = GetNode(AttachedId(sender));
            if (n == null) return;

            var tag = n.Tag as NodeEditorCore.ShaderFragmentNodeTag;
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
            var subgraph = ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            using (var fm = new NodeEditorCore.InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    subgraph.AddItem(new NodeEditorCore.ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type) { Semantic = fm.Semantic, Default = fm.Default, Direction = NodeEditorCore.InterfaceDirection.In }, HyperGraph.Node.Dock.Output);
                }
            }
        }

        private void OnCreateOutputParameterNode(object sender, EventArgs e)
        {
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            using (var fm = new NodeEditorCore.InterfaceParameterForm(false) { Name = "Color", Type = "auto", Semantic = "" })
            {
                var result = fm.ShowDialog();
                if (result == DialogResult.OK)
                {
                    subgraph.AddItem(new NodeEditorCore.ShaderFragmentInterfaceParameterItem(fm.Name, fm.Type) { Semantic = fm.Semantic, Default = fm.Default, Direction = NodeEditorCore.InterfaceDirection.Out }, HyperGraph.Node.Dock.Input);
                }
            }
        }

        private void OnCreateCapturesGroup(object sender, EventArgs e)
        {
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            using (var dialog = new HyperGraph.TextEditForm { InputText = "Uniforms" })
            {
                var result = dialog.ShowDialog();
                if (result == DialogResult.OK)
                {
                    var node = _nodeFactory.CreateCapturesNode(dialog.InputText, Enumerable.Empty<GUILayer.NodeGraphSignature.Parameter>());
                    node.SubGraphTag = subgraph.SubGraphTag;
                    ViewModel.AddNode(node);
                }
            }
        }

        private void OnSetSubGraphProperties(object sender, EventArgs e)
        {
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            _nodeFactory.GetSubGraphProperties(subgraph, out string name, out string implements);

            using (var dialog = new NodeEditorCore.SubGraphPropertiesForm { Name = name, Implements = implements })
            {
                var result = dialog.ShowDialog();
                if (result == DialogResult.OK)
                {
                    if (!dialog.Name.Equals(name) && HasSubGraphNamed(dialog.Name))
                    {
                        MessageBox.Show("Cannot rename sub-graph to (" + dialog.Name + ") because a sub-graph with that name already exists");
                    }
                    else
                    {
                        _nodeFactory.SetSubGraphProperties(Document.NodeGraphFile, subgraph, dialog.Name, dialog.Implements);

                        var newTag = subgraph.SubGraphTag;
                        foreach (var node in ViewModel.Nodes)
                            if (node.SubGraphTag == subgraphTag)
                                node.SubGraphTag = newTag;
                        ViewModel.InvokeMiscChange(true);
                    }
                }
            }
        }

        private void OnDeleteSubGraph(object sender, EventArgs e)
        {
            object subgraphTag = AttachedSubGraphTag(sender);
            var subgraph = ViewModel.SubGraphs.Where(x => x.SubGraphTag == subgraphTag).FirstOrDefault();
            if (subgraph == null) return;

            List<HyperGraph.Node> nodesToRemove = new List<HyperGraph.Node>();
            foreach (var node in ViewModel.Nodes)
                if (node.SubGraphTag == subgraphTag)
                    nodesToRemove.Add(node);
            nodesToRemove.Add(subgraph);
            ViewModel.RemoveNodes(nodesToRemove);
        }

        private bool HasSubGraphNamed(string name)
        {
            return ViewModel.SubGraphs.Where(x => string.Compare(x.SubGraphTag as string, name) == 0).Any();
        }

        private void OnCreateSubGraph(object sender, EventArgs e)
        {
            // pick out a unique name to seed the entry box
            string name = "main";
            if (HasSubGraphNamed(name))
            {
                for (int c = 0; ; ++c)
                {
                    name = "subgraph" + c.ToString();
                    if (!HasSubGraphNamed(name)) break;
                }
            }

            using (var dialog = new NodeEditorCore.SubGraphPropertiesForm { Name = name })
            {
                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    if (!HasSubGraphNamed(dialog.Name))
                    {
                        var n = _nodeFactory.CreateSubGraph(Document.NodeGraphFile, dialog.Name, dialog.Implements);
                        var tag = ((ToolStripMenuItem)sender).GetCurrentParent().Tag;
                        if (tag is System.Drawing.PointF)
                            n.Location = (System.Drawing.PointF)tag;
                        ViewModel.AddSubGraph(n);
                    }
                    else
                    {
                        MessageBox.Show("Cannot create sub-graph with name (" + dialog.Name + ") because a sub-graph with that name already exists");
                    }
                }
            }
        }

        #endregion

        private ContextMenuStrip _createSubGraphMenu;
        private ContextMenuStrip _emptySpaceMenu;
        private System.ComponentModel.Container _components;

        #endregion

        [Import] private ControlsLibraryExt.Material.ActiveMaterialContext _activeMaterialContext;
        [Import] private NodeEditorCore.INodeFactory _nodeFactory;
        private HoverLabel _hover = null;
        private AdaptableSet _contextSet;
    }
}
