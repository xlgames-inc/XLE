// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;
using System.Data;
using System.Drawing;
using System.Windows.Forms;
using System.Linq;      // (only used for IEnumerable.ToList below)
using HyperGraph;

using RibbonLib;
using RibbonLib.Controls;
using RibbonLib.Controls.Events;
using RibbonLib.Interop;

namespace NodeEditor
{
    [Export(typeof(ExampleForm))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
	public partial class ExampleForm : Form
	{
        public enum RibbonMarkupCommands : uint
        {
            cmdStandardNodesGallery  = 1007,
            cmdTabGroupTextureNode   = 1024,
            cmdShowLabels            = 1026,
            cmdButtonTestScript      = 1008,
            cmdSaveAs                = 1009,
            cmdLoad                  = 1010
        }

        [ImportingConstructor]
		public ExampleForm(ExportProvider exportProvider)
		{
            _shaderFragments = exportProvider.GetExport<ShaderFragmentArchive.Archive>().Value;
            _nodeCreator = exportProvider.GetExport<NodeEditorCore.INodeFactory>().Value;
            _modelConversion = exportProvider.GetExport<NodeEditorCore.IModelConversion>().Value;

			InitializeComponent();

            _hyperGraphModel = new HyperGraph.GraphModel();
            _hyperGraphModel.CompatibilityStrategy = _nodeCreator.CreateCompatibilityStrategy();

            _graphAdapter = new HyperGraph.GraphControl();
            _graphAdapter.Attach(graphControl);
            _graphAdapter.HighlightCompatible = true;
            _graphAdapter.LargeGridStep = 160F;
            _graphAdapter.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(90)))), ((int)(((byte)(90)))), ((int)(((byte)(90)))));
            _graphAdapter.ShowLabels = false;
            _graphAdapter.SmallGridStep = 20F;
            _graphAdapter.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(80)))), ((int)(((byte)(80)))), ((int)(((byte)(80)))));

            _graphAdapter.Model = _hyperGraphModel;
            _graphAdapter.Selection = new GraphSelection();
            _graphAdapter.Selection.SelectionChanged += OnFocusChanged;

            graphControl.MouseEnter += OnGraphMouseEnter;
            graphControl.Paint += graphControl_Paint;

            _tabGroupTextureNode = new RibbonTabGroup(_ribbon, (uint)RibbonMarkupCommands.cmdTabGroupTextureNode);

            _showLabels = new RibbonLib.Controls.RibbonCheckBox(_ribbon, (uint)RibbonMarkupCommands.cmdShowLabels);
            _showLabels.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnShowLabelsChanged);

            _generateTestScript = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdButtonTestScript);
            _generateTestScript.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnGenerateTestScript);

            _saveAsButton = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdSaveAs);
            _saveAsButton.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnSaveAs);

            _loadButton = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdLoad);
            _loadButton.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnLoad);

            var fragmentTreeModel = exportProvider.GetExport<NodeEditorCore.ShaderFragmentArchiveModel>().Value;
            _fragmentTree.Model = new Aga.Controls.Tree.SortedTreeModel(fragmentTreeModel);
            _fragmentTree.ItemDrag += new ItemDragEventHandler(OnFragmentTreeItemDrag);
            _fragmentTree.NodeMouseDoubleClick += new EventHandler<Aga.Controls.Tree.TreeNodeAdvMouseEventArgs>(OnFragmentTreeItemDoubleClick);

            // _materialParametersGrid.SelectedObject = new DictionaryPropertyGridAdapter(_document.PreviewMaterialState);

            try
            {
                LoadFile("defaultload.sh");
            }
            catch(System.Exception) {}
		}

        void graphControl_Paint(object sender, PaintEventArgs e)
        {
            var engine = GUILayer.EngineDevice.GetInstance();
            engine.ForegroundUpdate();
        }

        private ShaderFragmentArchive.Archive _shaderFragments;
        private NodeEditorCore.INodeFactory _nodeCreator;
        private NodeEditorCore.IModelConversion _modelConversion;
        private HyperGraph.GraphControl _graphAdapter;

        #region Graph control event handlers
        void OnFocusChanged(object sender, EventArgs e)
        {
            var textureContext = ContextAvailability.NotAvailable;

            var sel = _graphAdapter.Selection.Selection;
            if (sel.FirstOrDefault() is Node)
            {
                var node = sel.FirstOrDefault() as Node;
                if (node.Title.Equals("Texture"))
                {
                    textureContext = ContextAvailability.Available;
                }
            }
        
            _tabGroupTextureNode.ContextAvailable = textureContext;
        }

        private void OnShowLabelsChanged(object sender, ExecuteEventArgs e)
		{
            _graphAdapter.ShowLabels = _showLabels.BooleanValue;
		}
        #endregion

        #region Fragment tree event handlers
        private void OnFragmentTreeItemDrag(object sender, ItemDragEventArgs e)
        {
            if (e != null)
            {
                if (e.Item is Aga.Controls.Tree.TreeNodeAdv[])
                {
                    var array = (Aga.Controls.Tree.TreeNodeAdv[])e.Item;
                    foreach (var a in array)
                    {
                        if (a.Tag is NodeEditorCore.ShaderFragmentArchiveModel.ShaderFragmentItem)
                        {
                            var item = (NodeEditorCore.ShaderFragmentArchiveModel.ShaderFragmentItem)a.Tag;

                            var archiveName = item.ArchiveName;
                            if (archiveName != null && archiveName.Length > 0)
                            {
                                var fn = _shaderFragments.GetFunction(archiveName);
                                if (fn != null)
                                {
                                    this.DoDragDrop(
                                        _nodeCreator.CreateNode(fn, archiveName), 
                                        DragDropEffects.Copy);
                                }
                            }
                        }
                        else if (a.Tag is NodeEditorCore.ShaderFragmentArchiveModel.ParameterStructItem)
                        {
                            var item = (NodeEditorCore.ShaderFragmentArchiveModel.ParameterStructItem)a.Tag;

                            var archiveName = item.ArchiveName;
                            if (archiveName != null && archiveName.Length > 0)
                            {
                                var fn = _shaderFragments.GetParameterStruct(archiveName);
                                if (fn != null)
                                {
                                    this.DoDragDrop(_nodeCreator.CreateParameterNode(fn, archiveName, ShaderFragmentArchive.Parameter.SourceType.Material), DragDropEffects.Copy);
                                }
                            }
                        }
                    }
                }
            }
        }

        private void OnFragmentTreeItemDoubleClick(object sender, Aga.Controls.Tree.TreeNodeAdvMouseEventArgs e)
        {
            // if (e.Node != null && e.Node.Tag is NodeEditorCore.ShaderFragmentArchiveModel.ParameterStructItem)
            // {
            //     NodeEditorCore.ShaderParameterUtil.EditParameter(_hyperGraphModel, ((NodeEditorCore.ShaderFragmentArchiveModel.ParameterStructItem)e.Node.Tag).ArchiveName);
            // }
        }
        #endregion

        #region File Input / Output
        private void OnGenerateTestScript(object sender, ExecuteEventArgs e)
        {
                //
                //      Convert the editor nodes into "ShaderPatcherLayer" representation
                //      This can be used for serialisation & for output to a shader
                //

            var nodeGraph = _modelConversion.ToShaderPatcherLayer(_hyperGraphModel);
            var shader = ShaderPatcherLayer.NodeGraph.GenerateShader(nodeGraph, "Test");
            MessageBox.Show(shader, "Generated shader", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
        }

        private void OnSaveAs(object sender, ExecuteEventArgs e)
        {
            using (SaveFileDialog dialog = new SaveFileDialog())
            {
                dialog.Filter = "Shader file (*.sh)|*.sh|All files (*.*)|*.*";
                dialog.FilterIndex = 0;
                dialog.RestoreDirectory = true;

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    var graph = _modelConversion.ToShaderPatcherLayer(_hyperGraphModel);
                    ShaderPatcherLayer.NodeGraph.Save(dialog.FileName, graph, _document);
                }
            }
        }

        private void OnLoad(object sender, ExecuteEventArgs e)
        {
            using (OpenFileDialog dialog = new OpenFileDialog())
            {
                dialog.Filter = "Shader file (*.sh)|*.sh|XML shader diagram (*.xml)|*.xml|All files (*.*)|*.*";
                dialog.FilterIndex = 0;
                dialog.RestoreDirectory = true;

                if (dialog.ShowDialog() == DialogResult.OK)
                    LoadFile(dialog.FileName);
            }
        }

        private void LoadFile(string filename)
        {
            ShaderPatcherLayer.NodeGraph graph;
            ShaderPatcherLayer.NodeGraph.Load(filename, out graph, out _document);
            _hyperGraphModel.RemoveNodes(_hyperGraphModel.Nodes.ToList());
            _modelConversion.AddToHyperGraph(graph, _hyperGraphModel);
        }
        #endregion

        #region Mouse movement handlers
        private void OnGraphMouseEnter(object sender, EventArgs e)
        {
            graphControl.Focus();
        }
        
        private void OnFragmentsMouseEnter(object sender, EventArgs e)
        {
            if (panel2.Parent == splitContainer1.Panel1)
            {
                var startingHeight = panel2.Height;
                splitContainer1.Panel1.Controls.Remove(panel2);
                panel2.Dock = System.Windows.Forms.DockStyle.None;
                panel2.Width = 800;
                panel2.Height = startingHeight;
                panel1.Controls.Add(panel2);
                panel1.Controls.SetChildIndex(panel2, 0);
            }
        }

        private void OnFragmentsMouseLeave(object sender, EventArgs e)
        {
            if (panel2.Parent == panel1)
            {
                panel1.Controls.Remove(panel2);
                panel2.Dock = System.Windows.Forms.DockStyle.Fill;
                splitContainer1.Panel1.Controls.Add(panel2);
            }
        }
        #endregion

        private void _materialParametersGrid_PropertyValueChanged(object s, PropertyValueChangedEventArgs e)
        {
            // NodeEditorCore.ShaderFragmentNodeUtil.InvalidateParameters(_hyperGraphModel);
            graphControl.Refresh();
        }

        #region Members
        private ShaderPatcherLayer.NodeGraphMetaData _document = new ShaderPatcherLayer.NodeGraphMetaData();
        private RibbonTabGroup _tabGroupTextureNode;
        private RibbonLib.Controls.RibbonCheckBox _showLabels;
        private RibbonLib.Controls.RibbonButton _generateTestScript;
        private RibbonLib.Controls.RibbonButton _saveAsButton;
        private RibbonLib.Controls.RibbonButton _loadButton;
        private HyperGraph.IGraphModel _hyperGraphModel;
        #endregion
	}
}
