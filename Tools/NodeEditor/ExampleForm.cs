// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
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

		public ExampleForm()
		{
			InitializeComponent();

            graphControl.CompatibilityStrategy = new ShaderFragmentNodeCompatibility();
            graphControl.ConnectionAdded	+= new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionAdded);
			graphControl.ConnectionAdding	+= new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionAdding);
			graphControl.ConnectionRemoving += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionRemoved);
            graphControl.NodeAdded          += new EventHandler<AcceptNodeEventArgs>(OnNodeAdded);
            graphControl.NodeRemoved        += new EventHandler<NodeEventArgs>(OnNodeRemoved);
            graphControl.ConnectionRemoving += new EventHandler<AcceptNodeConnectionEventArgs>(OnConnectionRemoved);
			graphControl.ShowElementMenu	+= new EventHandler<AcceptElementLocationEventArgs>(OnShowElementMenu);
            graphControl.FocusChanged       += new EventHandler<ElementEventArgs>(OnFocusChanged);
            graphControl.MouseEnter         += new System.EventHandler(OnGraphMouseEnter);
            graphControl.ConnectorDoubleClick += new EventHandler<HyperGraph.GraphControl.NodeConnectorEventArgs>(OnConnectorDoubleClick);

            _tabGroupTextureNode = new RibbonTabGroup(_ribbon, (uint)RibbonMarkupCommands.cmdTabGroupTextureNode);

            _showLabels = new RibbonLib.Controls.RibbonCheckBox(_ribbon, (uint)RibbonMarkupCommands.cmdShowLabels);
            _showLabels.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnShowLabelsChanged);

            _generateTestScript = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdButtonTestScript);
            _generateTestScript.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnGenerateTestScript);

            _saveAsButton = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdSaveAs);
            _saveAsButton.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnSaveAs);

            _loadButton = new RibbonLib.Controls.RibbonButton(_ribbon, (uint)RibbonMarkupCommands.cmdLoad);
            _loadButton.ExecuteEvent += new EventHandler<ExecuteEventArgs>(OnLoad);

            _fragmentTree.Model = new Aga.Controls.Tree.SortedTreeModel(new TreeViewArchiveModel());
            _fragmentTree.ItemDrag += new ItemDragEventHandler(OnFragmentTreeItemDrag);
            _fragmentTree.NodeMouseDoubleClick += new EventHandler<Aga.Controls.Tree.TreeNodeAdvMouseEventArgs>(OnFragmentTreeItemDoubleClick);

            _materialParametersGrid.SelectedObject = new DictionaryPropertyGridAdapter(_document.PreviewMaterialState);

            try
            {
                System.IO.FileStream fileStream = new System.IO.FileStream("defaultload.sh", System.IO.FileMode.Open);
                try
                {
                    LoadFromShader(fileStream);
                }
                finally
                {
                    fileStream.Close();
                }
            }
            catch (System.Exception) {}
		}

        #region Graph control event handlers
        void OnFocusChanged(object sender, ElementEventArgs e)
        {
            var textureContext = ContextAvailability.NotAvailable;

            if (e.Element is Node)
            {
                var node = (Node)e.Element;
                if (node.Title.Equals("Texture"))
                {
                    textureContext = ContextAvailability.Available;
                }
            }

            _tabGroupTextureNode.ContextAvailable = textureContext;
        }

        private void OnNodeAdded(object sender, AcceptNodeEventArgs args) { OnNodesChange(); }
        private void OnNodeRemoved(object sender, NodeEventArgs args) { OnNodesChange(); ShaderFragmentNodeUtil.InvalidateShaderStructure(graphControl); }
        private void OnNodesChange()
        {
            var didSomething = ShaderParameterUtil.FillInMaterialParameters(_document, graphControl);
            if (didSomething)
            {
                _materialParametersGrid.Refresh();
            }
        }

		void OnConnectionAdding(object sender, AcceptNodeConnectionEventArgs e) {}

		static int counter = 1;
		void OnConnectionAdded(object sender, AcceptNodeConnectionEventArgs e)
		{
			e.Connection.Name = "Connection " + counter ++;
            ShaderFragmentNodeUtil.InvalidateShaderStructure(graphControl);
		}

        void OnConnectionRemoved(object sender, AcceptNodeConnectionEventArgs e)
        {
            ShaderFragmentNodeUtil.InvalidateShaderStructure(graphControl);
        }

        void OnConnectorDoubleClick(object sender, HyperGraph.GraphControl.NodeConnectorEventArgs e)
        {
            var dialog = new HyperGraph.TextEditForm();
            dialog.InputText = "1.0f";

                //  look for an existing simple connection attached to this connector
            foreach(var i in e.Node.Connections) 
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
                        i.Name = dialog.InputText;
                        foundExisting = true;
                    }

                if (!foundExisting)
                {
                    var connection = new NodeConnection();
                    connection.To = e.Connector;
                    connection.Name = dialog.InputText;
                    e.Node.AddConnection(connection);
                }

                ShaderFragmentNodeUtil.InvalidateAttachedConstants(graphControl);
            }
        }

        private void OnShowLabelsChanged(object sender, ExecuteEventArgs e)
		{
            graphControl.ShowLabels = _showLabels.BooleanValue;
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
                        if (a.Tag is TreeViewArchiveModel.ShaderFragmentItem)
                        {
                            var item = (TreeViewArchiveModel.ShaderFragmentItem)a.Tag;

                            var archiveName = item.ArchiveName;
                            if (archiveName != null && archiveName.Length > 0)
                            {
                                var fn = ShaderFragmentArchive.Archive.GetFunction(archiveName);
                                if (fn != null)
                                {
                                    this.DoDragDrop(ShaderFragmentNodeCreator.CreateNode(fn, archiveName, graphControl, _document), DragDropEffects.Copy);
                                }
                            }
                        }
                        else if (a.Tag is TreeViewArchiveModel.ParameterStructItem)
                        {
                            var item = (TreeViewArchiveModel.ParameterStructItem)a.Tag;

                            var archiveName = item.ArchiveName;
                            if (archiveName != null && archiveName.Length > 0)
                            {
                                var fn = ShaderFragmentArchive.Archive.GetParameterStruct(archiveName);
                                if (fn != null)
                                {
                                    this.DoDragDrop(ShaderFragmentNodeCreator.CreateParameterNode(fn, archiveName, ShaderFragmentArchive.Parameter.SourceType.Material), DragDropEffects.Copy);
                                }
                            }
                        }
                    }
                }
            }
        }

        private void OnFragmentTreeItemDoubleClick(object sender, Aga.Controls.Tree.TreeNodeAdvMouseEventArgs e)
        {
            if (e.Node != null && e.Node.Tag is TreeViewArchiveModel.ParameterStructItem)
            {
                ShaderParameterUtil.EditParameter(graphControl, ((TreeViewArchiveModel.ParameterStructItem)e.Node.Tag).ArchiveName);
            }
        }
        #endregion

        #region File Input / Output
        private void OnGenerateTestScript(object sender, ExecuteEventArgs e)
        {
                //
                //      Convert the editor nodes into "ShaderPatcherLayer" representation
                //      This can be used for serialisation & for output to a shader
                //

            var nodeGraph    = ModelConversion.ToShaderPatcherLayer(graphControl);
            var shader       = ShaderPatcherLayer.NodeGraph.GenerateShader(nodeGraph, "Test");
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
                    using (var stream = dialog.OpenFile())
                    {
                        using (var xmlStream = new System.IO.MemoryStream())
                        {
                            var nodeGraph = ModelConversion.ToShaderPatcherLayer(graphControl);
                            var serializer = new System.Runtime.Serialization.DataContractSerializer(
                                typeof(ShaderPatcherLayer.NodeGraph));
                            var settings = new System.Xml.XmlWriterSettings()
                            {
                                Indent = true,
                                IndentChars = "\t",
                                Encoding = System.Text.Encoding.ASCII
                            };

                                // write the xml to a memory stream to begin with
                            using (var writer = System.Xml.XmlWriter.Create(xmlStream, settings))
                            {
                                serializer.WriteObject(writer, nodeGraph);
                            }

                                // we hide the memory stream within a comment, and write
                                // out a hlsl shader file
                                // The HLSL compiler doesn't like UTF files... It just wants plain ASCII encoding

                            using (var sw = new System.IO.StreamWriter(stream, System.Text.Encoding.ASCII))
                            {
                                var shader = ShaderPatcherLayer.NodeGraph.GenerateShader(nodeGraph, System.IO.Path.GetFileNameWithoutExtension(dialog.FileName));
                                sw.Write(shader); 
                                
                                sw.Write("/* **** **** NodeEditor **** **** \r\nNEStart{");
                                sw.Flush();
                                xmlStream.WriteTo(stream);
                                sw.Write("}NEEnd\r\n **** **** */\r\n");
                            }

                        }
                    }
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
                {
                    using (var stream = dialog.OpenFile())
                    {
                        if (string.Compare(System.IO.Path.GetExtension(dialog.FileName), ".xml", true) == 0)
                        {
                            LoadFromXML(stream);
                        }
                        else 
                        {
                            LoadFromShader(stream);
                        }
                    }
                }
            }
        }

        private bool LoadFromXML(System.IO.Stream stream)
        {
            var serializer = new System.Runtime.Serialization.DataContractSerializer(
                            typeof(ShaderPatcherLayer.NodeGraph));
            using (var xmlStream = System.Xml.XmlReader.Create(stream))
            {
                var o = serializer.ReadObject(xmlStream);
                if (o != null && o is ShaderPatcherLayer.NodeGraph)
                {
                    graphControl.RemoveNodes(graphControl.Nodes.ToList());
                    ModelConversion.AddToHyperGraph((ShaderPatcherLayer.NodeGraph)o, graphControl, _document);
                    return true;
                }
            }
            return false;
        }

        private bool LoadFromShader(System.IO.Stream stream)
        {
                // the xml should be hidden within a comment in this file.
            //      look for a string between "NEStart{" XXX "}NEEnd"

            using (var sr = new System.IO.StreamReader(stream, System.Text.Encoding.ASCII))
            {
                var asString = sr.ReadToEnd();
                var matches = System.Text.RegularExpressions.Regex.Matches(asString, 
                    @"NEStart\{(.*)\}NEEnd", 
                    System.Text.RegularExpressions.RegexOptions.CultureInvariant | System.Text.RegularExpressions.RegexOptions.Singleline);
                if (matches.Count > 0 && matches[0].Groups.Count > 1)
                {
                    var xmlString = matches[0].Groups[1].Value;
                    return LoadFromXML(new System.IO.MemoryStream(System.Text.Encoding.ASCII.GetBytes(xmlString)));
                }
            }

            return false;
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
            if (e.Element is Node && ((Node)e.Element).Tag is ShaderParameterNodeTag)
            {
                var tag = (ShaderParameterNodeTag)((Node)e.Element).Tag;
                parameterBoxMenu.Tag = tag.Id;
                parameterBoxMenu.Show(e.Position);
                e.Cancel = false;
            }
            else if (e.Element is ShaderFragmentNodeItem)
            {
                var tag = (ShaderFragmentNodeItem)e.Element;
                if (tag.ArchiveName != null)
                {
                    ShaderParameterUtil.EditParameter(graphControl, tag.ArchiveName);
                    e.Cancel = false;
                }
            }
            else if (e.Element is NodeConnector && ((NodeConnector)e.Element).Item is ShaderFragmentNodeItem)
            {
                var tag = (ShaderFragmentNodeItem)((NodeConnector)e.Element).Item;
                if (tag.ArchiveName != null)
                {
                    ShaderParameterUtil.EditParameter(graphControl, tag.ArchiveName);
                    e.Cancel = false;
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
                {
                    return (UInt32)tag;
                }
            }
            return 0;
        }
        
        private void OnShowPreviewShader(object sender, EventArgs e)
        {
            var nodeGraph = ModelConversion.ToShaderPatcherLayer(graphControl);
            var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(nodeGraph, AttachedId(sender), "");
            MessageBox.Show(shader, "Generated shader", MessageBoxButtons.OK, MessageBoxIcon.Asterisk);
        }

        private void refreshToolStripMenuItem_Click(object sender, EventArgs e)
        {
                //      Find the attached preview items and invalidate
            var n = ShaderFragmentNodeUtil.GetShaderFragmentNode(graphControl, AttachedId(sender));
            if (n!=null) {
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
            var n = ShaderFragmentNodeUtil.GetShaderFragmentNode(graphControl, AttachedId(sender));
            if (n != null)
            {
                var nodeId = ((ShaderFragmentNodeTag)n.Tag).Id;

                    // generate a preview builder for this specific node...
                var nodeGraph = ModelConversion.ToShaderPatcherLayer(graphControl);
                var shader = ShaderPatcherLayer.NodeGraph.GeneratePreviewShader(nodeGraph, nodeId, "");
                var builder = PreviewRender.Manager.Instance.CreatePreview(shader);

                    // create a "LargePreview" window
                new LargePreview(builder, _document).Show();
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

        private void _materialParametersGrid_PropertyValueChanged(object s, PropertyValueChangedEventArgs e)
        {
            ShaderFragmentNodeUtil.InvalidateParameters(graphControl);
            graphControl.Refresh();
        }
        #endregion

        #region Members
        private ShaderDiagram.Document _document = new ShaderDiagram.Document();
        private RibbonTabGroup _tabGroupTextureNode;
        private RibbonLib.Controls.RibbonCheckBox _showLabels;
        private RibbonLib.Controls.RibbonButton _generateTestScript;
        private RibbonLib.Controls.RibbonButton _saveAsButton;
        private RibbonLib.Controls.RibbonButton _loadButton;
        #endregion
	}
}
