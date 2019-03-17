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

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace MaterialTool.Controls
{
    interface IGraphControl
    {
        void SetContext(DiagramEditingContext context);
    }

    class AdaptableSet : IAdaptable, IDecoratable
    {
        public object GetAdapter(Type type)
        {
            foreach(var o in _subObjects)
            {
                var r = o.As(type);
                if (r != null) return r;
            }
            return null;
        }

        public IEnumerable<object> GetDecorators(Type type)
        {
            foreach (var o in _subObjects)
            {
                var d = o.As<IDecoratable>();
                if (d==null) continue;
                var l = d.GetDecorators(type);
                foreach (var i in l) yield return i;
            }
        }

        public AdaptableSet(IEnumerable<object> subObjects) { _subObjects = subObjects; }

        private IEnumerable<object> _subObjects;
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

            var graphAdapter = new HyperGraphAdapter();
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
                    new PickingAdapter { Context = context },
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
            var shader = graphContext.Document.NodeGraphFile.GeneratePreviewShader(
                p.SubGraph, p.NodeId, p.PreviewSettings,
                graphContext.Document.GraphMetaData.Variables);

            ControlsLibrary.BasicControls.TextWindow.Show(
                System.Text.RegularExpressions.Regex.Replace(shader.Item1, @"\r\n|\n\r|\n|\r", "\r\n"));        // (make sure we to convert the line endings into windows form)
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
                System.Diagnostics.Debug.Assert(false);
                // doc.ContainingDocument.Invalidate();
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
                    var node = _nodeFactory.CreateCapturesNode(dialog.InputText, Enumerable.Empty<ShaderPatcherLayer.NodeGraphSignature.Parameter>());
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

    class HyperGraphAdapter : HyperGraph.GraphControl, IControlAdapter, ITransformAdapter
    {
        public AdaptableControl AdaptedControl { get; set; }

        public void Bind(AdaptableControl control)
        {
            Unbind(AdaptedControl);
            AdaptedControl = control;
            Attach(control);
        }
        public void BindReverse(AdaptableControl control) { }
        public void Unbind(AdaptableControl control)
        {
            if (control == null) return;
            Detach(control);
        }

        #region ITransformAdapter
        Matrix ITransformAdapter.Transform
        {
            get { return base.Transform; }
        }

        public PointF Translation
        {
            get { return new PointF(Transform.OffsetX, Transform.OffsetY); }
            set { SetTranslation(value); }
        }

        public PointF Scale
        {
            get
            {
                float[] m = Transform.Elements;
                return new PointF(m[0], m[3]);
            }
            set
            {
                SetScale(value);
            }
        }

        public bool EnforceConstraints
        {
            set { _enforceConstraints = value; }
            get { return _enforceConstraints; }
        }

        public PointF MinTranslation
        {
            get { return _minTranslation; }
            set
            {
                _minTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MaxTranslation
        {
            get { return _maxTranslation; }
            set
            {
                _maxTranslation = value;
                SetTranslation(Translation); // check translation against new constraints
            }
        }

        public PointF MinScale
        {
            get { return _minScale; }
            set
            {
                if (value.X <= 0 ||
                    value.X > _maxScale.X ||
                    value.Y <= 0 ||
                    value.Y > _maxScale.Y)
                {
                    throw new ArgumentException("minimum components must be > 0 and less than maximum");
                }

                _minScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public PointF MaxScale
        {
            get { return _maxScale; }
            set
            {
                if (value.X < _minScale.X ||
                    value.Y < _minScale.Y)
                {
                    throw new ArgumentException("maximum components must be greater than minimum");
                }

                _maxScale = value;
                SetScale(Scale); // check scale against new constraints
            }
        }

        public bool UniformScale
        {
            get { return _uniformScale; }
            set { _uniformScale = value; }
        }

        public void SetTransform(float xScale, float yScale, float xTranslation, float yTranslation)
        {
            PointF scale = EnforceConstraints ? this.ConstrainScale(new PointF(xScale, yScale)) : new PointF(xScale, yScale);
            PointF translation = EnforceConstraints ? this.ConstrainTranslation(new PointF(xTranslation, yTranslation)) : new PointF(xTranslation, yTranslation);
            SetTransformInternal((scale.X + scale.Y) * 0.5f, translation.X, translation.Y);
        }

        private void SetTranslation(PointF translation)
        {
            translation = EnforceConstraints ? this.ConstrainTranslation(translation) : translation;
            SetTransformInternal(_zoom, translation.X, translation.Y);
        }

        private void SetScale(PointF scale)
        {
            scale = EnforceConstraints ? this.ConstrainScale(scale) : scale;
            SetTransformInternal((scale.X + scale.Y) * 0.5f, _translation.X, _translation.Y);
        }

        public void SetTransformInternal(float zoom, float xTranslation, float yTranslation)
        {
            bool transformChanged = false;
            if (_zoom != zoom)
            {
                _zoom = zoom;
                transformChanged = true;
            }
            
            if (_translation.X != xTranslation || _translation.Y != xTranslation)
            {
                _translation = new PointF(xTranslation, yTranslation);
                transformChanged = true;
            }

            if (transformChanged)
            {
                UpdateMatrices();
                TransformChanged.Raise(this, EventArgs.Empty);
                if (AdaptedControl != null)
                    AdaptedControl.Invalidate();
            }
        }

        public event EventHandler TransformChanged;

        private PointF _minTranslation = new PointF(float.MinValue, float.MinValue);
        private PointF _maxTranslation = new PointF(float.MaxValue, float.MaxValue);
        private PointF _minScale = new PointF(float.MinValue, float.MinValue);
        private PointF _maxScale = new PointF(float.MaxValue, float.MaxValue);
        private bool _uniformScale = true;
        private bool _enforceConstraints = false;
        #endregion
    }

    class PickingAdapter : ControlAdapter, IPickingAdapter2
    {
        internal DiagramEditingContext Context;

        public DiagramHitRecord Pick(Point p)
        {
            return new DiagramHitRecord(HyperGraph.GraphControl.FindElementAt(Context.ViewModel, p));
        }

        public IEnumerable<object> Pick(Rectangle bounds)
        {
            RectangleF rectF = new RectangleF((float)bounds.X, (float)bounds.Y, (float)bounds.Width, (float)bounds.Height);
            return HyperGraph.GraphControl.RectangleSelection(Context.ViewModel, rectF);
        }

        public Rectangle GetBounds(IEnumerable<object> items)
        {
            float minX = float.MaxValue, minY = float.MaxValue;
            float maxX = -float.MaxValue, maxY = -float.MaxValue;

            foreach (var o in items)
            {
                var n = o as HyperGraph.Node;
                if (n != null)
                {
                    minX = System.Math.Min(minX, n.bounds.Left);
                    minY = System.Math.Min(minY, n.bounds.Top);
                    maxX = System.Math.Max(maxX, n.bounds.Right);
                    maxY = System.Math.Max(maxY, n.bounds.Bottom);
                }
            }

            // interface wants the result in client coords, so we need to transform through
            // the canvas transformation.
            var graphControl = AdaptedControl.As<HyperGraph.GraphControl>();
            if (graphControl != null) {
                var pts = new PointF[] { new PointF(minX, minY), new PointF(maxX, maxY) };
                graphControl.Transform.TransformPoints(pts);
                return new Rectangle((int)pts[0].X, (int)pts[0].Y, (int)(pts[1].X - pts[0].X), (int)(pts[1].Y - pts[0].Y));
            }
            return new Rectangle((int)minX, (int)minY, (int)(maxX - minX), (int)(maxY - minY));
        }
    }

    public class ViewingContext : IViewingContext, ILayoutContext
    {
        public AdaptableControl Control
        {
            get { return _control; }
            set
            {
                if (_control != null)
                {
                    _control.SizeChanged -= control_SizeChanged;
                    _control.VisibleChanged -= control_VisibleChanged;
                }

                _control = value;
                _layoutConstraints = EmptyEnumerable<ILayoutConstraint>.Instance;

                if (_control != null)
                {
                    _layoutConstraints = _control.AsAll<ILayoutConstraint>();
                    _control.SizeChanged += control_SizeChanged;
                    _control.VisibleChanged += control_VisibleChanged;
                }

                SetCanvasBounds();
            }
        }

        public Rectangle GetClientSpaceBounds(object item)
        {
            return GetClientSpaceBounds(new object[] { item });
        }

        public Rectangle GetClientSpaceBounds(IEnumerable<object> items)
        {
            Rectangle bounds = new Rectangle();
            foreach (IPickingAdapter2 pickingAdapter in _control.AsAll<IPickingAdapter2>())
            {
                Rectangle adapterBounds = pickingAdapter.GetBounds(items);
                if (!adapterBounds.IsEmpty)
                {
                    if (bounds.IsEmpty)
                        bounds = adapterBounds;
                    else
                        bounds = Rectangle.Union(bounds, adapterBounds);
                }
            }

            return bounds;
        }

        private HyperGraph.IGraphModel GetViewModel()
        {
            var adapter = _control.As<HyperGraphAdapter>();
            if (adapter == null) return null;
            return adapter.Model;
        }

        public Rectangle GetClientSpaceBounds()
        {
            var items = new List<object>();
            var graph = GetViewModel();
            if (graph != null)
                items.AddRange(graph.Nodes);
            return GetClientSpaceBounds(items);
        }

        public IEnumerable<object> GetVisibleItems()
        {
            Rectangle windowBounds = _control.As<ICanvasAdapter>().WindowBounds;
            foreach (IPickingAdapter2 pickingAdapter in _control.AsAll<IPickingAdapter2>())
            {
                foreach (object item in pickingAdapter.Pick(windowBounds))
                    yield return item;
            }
        }

        #region IViewingContext Members
        public bool CanFrame(IEnumerable<object> items)
        {
            return _control.As<IViewingContext>().CanFrame(items);
        }
        public void Frame(IEnumerable<object> items)
        {
            _control.As<IViewingContext>().Frame(items);
        }
        public bool CanEnsureVisible(IEnumerable<object> items)
        {
            return _control.As<IViewingContext>().CanFrame(items);
        }
        public void EnsureVisible(IEnumerable<object> items)
        {
            _control.As<IViewingContext>().EnsureVisible(items);
        }
        #endregion

        #region ILayoutContext Members

        private BoundsSpecified GetNodeBounds(HyperGraph.Node element, out Rectangle bounds)
        {
            bounds = GetClientSpaceBounds(element);

            //transform to world coordinates
            // (note -- these transforms could be avoided by calculating the bounds in canvas space directly, without using the PickingAdapter)
            var transformAdapter = _control.As<ITransformAdapter>();
            bounds = GdiUtil.InverseTransform(transformAdapter.Transform, bounds);

            return BoundsSpecified.All;
        }

        BoundsSpecified ILayoutContext.GetBounds(object item, out Rectangle bounds)
        {
            var element = item.As<HyperGraph.Node>();
            if (element != null)
                return GetNodeBounds(element, out bounds);

            bounds = new Rectangle();
            return BoundsSpecified.None;
        }

        BoundsSpecified ILayoutContext.CanSetBounds(object item)
        {
            if (item.Is<HyperGraph.Node>())
                return BoundsSpecified.Location;
            return BoundsSpecified.None;
        }

        void ILayoutContext.SetBounds(object item, Rectangle bounds, BoundsSpecified specified)
        {
            var element = item.As<HyperGraph.Node>();
            if (element != null)
            {
                Rectangle workingBounds;
                var bs = GetNodeBounds(element, out workingBounds);
                if (bs == BoundsSpecified.Location || bs == BoundsSpecified.All)
                {
                    workingBounds = WinFormsUtil.UpdateBounds(workingBounds, bounds, specified);
                    element.Location = workingBounds.Location;
                }
            }
        }
        #endregion

        private void control_VisibleChanged(object sender, EventArgs e)
        {
            SetCanvasBounds();
        }

        private void control_SizeChanged(object sender, EventArgs e)
        {
            SetCanvasBounds();
        }

        /// <summary>
        /// Updates the control CanvasAdapter's bounds</summary>
        protected virtual void SetCanvasBounds()
        {
            // update the control CanvasAdapter's bounds
            if (_control != null && _control.Visible)
            {
                Rectangle bounds = GetClientSpaceBounds();

                //transform to world coordinates
                var transformAdapter = _control.As<ITransformAdapter>();
                bounds = GdiUtil.InverseTransform(transformAdapter.Transform, bounds);

                // Make the canvas larger than it needs to be to give the user some room.
                // Use the client rectangle in world coordinates.
                Rectangle clientRect = GdiUtil.InverseTransform(transformAdapter.Transform, _control.ClientRectangle);
                bounds.Width = Math.Max(bounds.Width * 2, clientRect.Width * 2);
                bounds.Height = Math.Max(bounds.Height * 2, clientRect.Height * 2);

                var canvasAdapter = _control.As<ICanvasAdapter>();
                if (canvasAdapter != null)
                    canvasAdapter.Bounds = bounds;
            }
        }

        private AdaptableControl _control;
        private IEnumerable<ILayoutConstraint> _layoutConstraints;
    }
}
