// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using Sce.Atf;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Applications;
using System.ComponentModel.Composition;
using System.Windows.Forms;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace MaterialTool.Controls
{
    [Export(typeof(ShaderFragmentArchiveControl))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    class ShaderFragmentArchiveControl : Control, IInitializable
    {
        void IInitializable.Initialize()
        {
            _controlHostService.RegisterControl(
                this,
                new ControlInfo(
                    "Shader Fragment Palette", 
                    "Palette of shader fragments, which can be dragged into the node diagram", 
                    StandardControlGroup.Right),
                null);
        }

        [ImportingConstructor]
        public ShaderFragmentArchiveControl(
            NodeEditorCore.ShaderFragmentArchiveModel archiveModel)
        {
            DoubleBuffered = false;

            Margin = new System.Windows.Forms.Padding(0);

            var fragmentTree = new Aga.Controls.Tree.TreeViewAdv();
            var treeColumn1 = new Aga.Controls.Tree.TreeColumn();
            var treeColumn3 = new Aga.Controls.Tree.TreeColumn();
            var treeColumn5 = new Aga.Controls.Tree.TreeColumn();
            var icon = new Aga.Controls.Tree.NodeControls.NodeStateIcon();
            var visibleName = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            var signature = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            var exceptionString = new Aga.Controls.Tree.NodeControls.NodeTextBox();

            // treeColumn1
            treeColumn1.Header = "Name";
            treeColumn1.SortOrder = System.Windows.Forms.SortOrder.None;
            treeColumn1.TooltipText = null;
            treeColumn1.Width = 300;
            // treeColumn3
            treeColumn3.Header = "Signature";
            treeColumn3.SortOrder = System.Windows.Forms.SortOrder.None;
            treeColumn3.TooltipText = null;
            treeColumn3.Width = 400;
            // treeColumn5
            treeColumn5.Header = "Exception";
            treeColumn5.SortOrder = System.Windows.Forms.SortOrder.None;
            treeColumn5.TooltipText = null;
            treeColumn5.Width = 200;
            // _icon
            icon.DataPropertyName = "Icon";
            icon.LeftMargin = 1;
            icon.ParentColumn = treeColumn1;
            icon.ScaleMode = Aga.Controls.Tree.ImageScaleMode.Clip;
            // _visibleName
            visibleName.DataPropertyName = "Name";
            visibleName.IncrementalSearchEnabled = true;
            visibleName.LeftMargin = 8;
            visibleName.ParentColumn = treeColumn1;
            visibleName.Trimming = System.Drawing.StringTrimming.EllipsisCharacter;
            // visibleName.Font = new System.Drawing.Font("Arial", 10F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            visibleName.UseCompatibleTextRendering = true;
            // _signature
            signature.DataPropertyName = "Signature";
            signature.IncrementalSearchEnabled = true;
            signature.LeftMargin = 3;
            signature.ParentColumn = treeColumn3;
            signature.UseCompatibleTextRendering = true;
            // _exceptionString
            exceptionString.DataPropertyName = "ExceptionString";
            exceptionString.IncrementalSearchEnabled = true;
            exceptionString.LeftMargin = 3;
            exceptionString.ParentColumn = treeColumn5;
            exceptionString.UseCompatibleTextRendering = true;
            // fragment tree
            fragmentTree.BackColor = System.Drawing.Color.Silver;
            fragmentTree.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            fragmentTree.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            fragmentTree.Columns.Add(treeColumn1);
            fragmentTree.Columns.Add(treeColumn3);
            fragmentTree.Columns.Add(treeColumn5);
            fragmentTree.DefaultToolTipProvider = null;
            fragmentTree.Dock = System.Windows.Forms.DockStyle.Fill;
            fragmentTree.DragDropMarkColor = System.Drawing.Color.Black;
            fragmentTree.Font = System.Drawing.SystemFonts.MenuFont;
                // new System.Drawing.Font("Arial", 10F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            fragmentTree.ForeColor = System.Drawing.Color.Gray;
            fragmentTree.FullRowSelect = true;
            fragmentTree.GridLineStyle = ((Aga.Controls.Tree.GridLineStyle)((Aga.Controls.Tree.GridLineStyle.Horizontal | Aga.Controls.Tree.GridLineStyle.Vertical)));
            fragmentTree.LineColor = System.Drawing.Color.White;
            fragmentTree.LoadOnDemand = true;
            fragmentTree.Location = new System.Drawing.Point(0, 0);
            fragmentTree.Model = null;
            fragmentTree.Name = "_fragmentTree";
            fragmentTree.NodeControls.Add(icon);
            fragmentTree.NodeControls.Add(visibleName);
            fragmentTree.NodeControls.Add(signature);
            fragmentTree.NodeControls.Add(exceptionString);
            fragmentTree.RowHeight = 32;
            fragmentTree.SelectedNode = null;
            fragmentTree.Size = new System.Drawing.Size(288, 311);
            fragmentTree.TabIndex = 3;
            fragmentTree.UseColumns = true;
            // fragmentTree.MouseEnter += new System.EventHandler(this.OnFragmentsMouseEnter);
            // fragmentTree.MouseLeave += new System.EventHandler(this.OnFragmentsMouseLeave);

            fragmentTree.Padding = new System.Windows.Forms.Padding(0);
            fragmentTree.Dock = System.Windows.Forms.DockStyle.Fill;

            fragmentTree.Model = new Aga.Controls.Tree.SortedTreeModel(archiveModel);
            fragmentTree.ItemDrag += new ItemDragEventHandler(OnFragmentTreeItemDrag);

            Controls.Add(fragmentTree);
        }

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
                                var fn = _fragments.GetFunction(archiveName, null);
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
                                var fn = _fragments.GetParameterStruct(archiveName, null);
                                if (fn != null)
                                {
                                    var node = _nodeCreator.CreateParameterNode(
                                        fn, archiveName, ShaderFragmentArchive.Parameter.SourceType.Material);
                                    this.DoDragDrop(node, DragDropEffects.Copy);
                                }
                            }
                        }
                    }
                }
            }
        }
        #endregion

        [Import(AllowDefault = false)]
        IControlHostService _controlHostService;

        [Import]
        ShaderFragmentArchive.Archive _fragments;

        [Import]
        NodeEditorCore.IShaderFragmentNodeCreator _nodeCreator;
    }
}
