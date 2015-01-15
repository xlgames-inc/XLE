namespace NodeEditor
{
	partial class ExampleForm
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            this.components = new System.ComponentModel.Container();
            HyperGraph.Compatibility.AlwaysCompatible alwaysCompatible1 = new HyperGraph.Compatibility.AlwaysCompatible();
            this.nodeMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.showPreviewShaderItem = new System.Windows.Forms.ToolStripMenuItem();
            this.refreshToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.graphControl = new HyperGraph.GraphControl();
            this.panel1 = new System.Windows.Forms.Panel();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.panel2 = new System.Windows.Forms.Panel();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this._fragmentTree = new Aga.Controls.Tree.TreeViewAdv();
            this.treeColumn1 = new Aga.Controls.Tree.TreeColumn();
            this.treeColumn3 = new Aga.Controls.Tree.TreeColumn();
            this.treeColumn4 = new Aga.Controls.Tree.TreeColumn();
            this.treeColumn5 = new Aga.Controls.Tree.TreeColumn();
            this._icon = new Aga.Controls.Tree.NodeControls.NodeStateIcon();
            this._visibleName = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._returnType = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._parameters = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._exceptionString = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._materialParametersGrid = new System.Windows.Forms.PropertyGrid();
            this._ribbon = new RibbonLib.Ribbon();
            this._materialPropertiesGrid = new System.Windows.Forms.PropertyGrid();
            this.parameterBoxMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.addParameterToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.largePreviewToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.nodeMenu.SuspendLayout();
            this.panel1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.panel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            this.parameterBoxMenu.SuspendLayout();
            this.SuspendLayout();
            // 
            // nodeMenu
            // 
            this.nodeMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.showPreviewShaderItem,
            this.refreshToolStripMenuItem,
            this.largePreviewToolStripMenuItem});
            this.nodeMenu.Name = "NodeMenu";
            this.nodeMenu.Size = new System.Drawing.Size(187, 92);
            // 
            // showPreviewShaderItem
            // 
            this.showPreviewShaderItem.Name = "showPreviewShaderItem";
            this.showPreviewShaderItem.Size = new System.Drawing.Size(186, 22);
            this.showPreviewShaderItem.Text = "Show Preview Shader";
            this.showPreviewShaderItem.Click += new System.EventHandler(this.OnShowPreviewShader);
            // 
            // refreshToolStripMenuItem
            // 
            this.refreshToolStripMenuItem.Name = "refreshToolStripMenuItem";
            this.refreshToolStripMenuItem.Size = new System.Drawing.Size(186, 22);
            this.refreshToolStripMenuItem.Text = "Refresh";
            this.refreshToolStripMenuItem.Click += new System.EventHandler(this.refreshToolStripMenuItem_Click);
            // 
            // graphControl
            // 
            this.graphControl.AllowDrop = true;
            this.graphControl.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(70)))), ((int)(((byte)(70)))), ((int)(((byte)(70)))));
            this.graphControl.CompatibilityStrategy = alwaysCompatible1;
            this.graphControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.graphControl.FocusElement = null;
            this.graphControl.HighlightCompatible = true;
            this.graphControl.LargeGridStep = 160F;
            this.graphControl.LargeStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(90)))), ((int)(((byte)(90)))), ((int)(((byte)(90)))));
            this.graphControl.Location = new System.Drawing.Point(0, 0);
            this.graphControl.Name = "graphControl";
            this.graphControl.ShowLabels = false;
            this.graphControl.Size = new System.Drawing.Size(996, 439);
            this.graphControl.SmallGridStep = 20F;
            this.graphControl.SmallStepGridColor = System.Drawing.Color.FromArgb(((int)(((byte)(80)))), ((int)(((byte)(80)))), ((int)(((byte)(80)))));
            this.graphControl.TabIndex = 0;
            this.graphControl.Text = "graphControl";
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.splitContainer1);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel1.Location = new System.Drawing.Point(0, 111);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(1300, 439);
            this.panel1.TabIndex = 7;
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.panel2);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.graphControl);
            this.splitContainer1.Size = new System.Drawing.Size(1300, 439);
            this.splitContainer1.SplitterDistance = 300;
            this.splitContainer1.TabIndex = 8;
            this.splitContainer1.TabStop = false;
            // 
            // panel2
            // 
            this.panel2.BackColor = System.Drawing.Color.Gray;
            this.panel2.Controls.Add(this.splitContainer2);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(0, 0);
            this.panel2.Name = "panel2";
            this.panel2.Padding = new System.Windows.Forms.Padding(6);
            this.panel2.Size = new System.Drawing.Size(300, 439);
            this.panel2.TabIndex = 2;
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(6, 6);
            this.splitContainer2.Name = "splitContainer2";
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this._fragmentTree);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this._materialParametersGrid);
            this.splitContainer2.Size = new System.Drawing.Size(288, 427);
            this.splitContainer2.SplitterDistance = 311;
            this.splitContainer2.TabIndex = 1;
            this.splitContainer2.TabStop = false;
            // 
            // _fragmentTree
            // 
            this._fragmentTree.BackColor = System.Drawing.Color.Silver;
            this._fragmentTree.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._fragmentTree.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._fragmentTree.Columns.Add(this.treeColumn1);
            this._fragmentTree.Columns.Add(this.treeColumn3);
            this._fragmentTree.Columns.Add(this.treeColumn4);
            this._fragmentTree.Columns.Add(this.treeColumn5);
            this._fragmentTree.DefaultToolTipProvider = null;
            this._fragmentTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this._fragmentTree.DragDropMarkColor = System.Drawing.Color.Black;
            this._fragmentTree.Font = new System.Drawing.Font("Cordia New", 16F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._fragmentTree.ForeColor = System.Drawing.Color.White;
            this._fragmentTree.FullRowSelect = true;
            this._fragmentTree.GridLineStyle = ((Aga.Controls.Tree.GridLineStyle)((Aga.Controls.Tree.GridLineStyle.Horizontal | Aga.Controls.Tree.GridLineStyle.Vertical)));
            this._fragmentTree.LineColor = System.Drawing.Color.White;
            this._fragmentTree.LoadOnDemand = true;
            this._fragmentTree.Location = new System.Drawing.Point(0, 0);
            this._fragmentTree.Model = null;
            this._fragmentTree.Name = "_fragmentTree";
            this._fragmentTree.NodeControls.Add(this._icon);
            this._fragmentTree.NodeControls.Add(this._visibleName);
            this._fragmentTree.NodeControls.Add(this._returnType);
            this._fragmentTree.NodeControls.Add(this._parameters);
            this._fragmentTree.NodeControls.Add(this._exceptionString);
            this._fragmentTree.RowHeight = 32;
            this._fragmentTree.SelectedNode = null;
            this._fragmentTree.Size = new System.Drawing.Size(288, 311);
            this._fragmentTree.TabIndex = 3;
            this._fragmentTree.UseColumns = true;
            this._fragmentTree.MouseEnter += new System.EventHandler(this.OnFragmentsMouseEnter);
            this._fragmentTree.MouseLeave += new System.EventHandler(this.OnFragmentsMouseLeave);
            // 
            // treeColumn1
            // 
            this.treeColumn1.Header = "Name";
            this.treeColumn1.SortOrder = System.Windows.Forms.SortOrder.None;
            this.treeColumn1.TooltipText = null;
            this.treeColumn1.Width = 300;
            // 
            // treeColumn3
            // 
            this.treeColumn3.Header = "ReturnType";
            this.treeColumn3.SortOrder = System.Windows.Forms.SortOrder.None;
            this.treeColumn3.TooltipText = null;
            this.treeColumn3.Width = 75;
            // 
            // treeColumn4
            // 
            this.treeColumn4.Header = "Parameters";
            this.treeColumn4.SortOrder = System.Windows.Forms.SortOrder.None;
            this.treeColumn4.TooltipText = null;
            this.treeColumn4.Width = 400;
            // 
            // treeColumn5
            // 
            this.treeColumn5.Header = "Exception";
            this.treeColumn5.SortOrder = System.Windows.Forms.SortOrder.None;
            this.treeColumn5.TooltipText = null;
            this.treeColumn5.Width = 200;
            // 
            // _icon
            // 
            this._icon.DataPropertyName = "Icon";
            this._icon.LeftMargin = 1;
            this._icon.ParentColumn = this.treeColumn1;
            this._icon.ScaleMode = Aga.Controls.Tree.ImageScaleMode.Clip;
            // 
            // _visibleName
            // 
            this._visibleName.DataPropertyName = "Name";
            this._visibleName.Font = new System.Drawing.Font("Cordia New", 15F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._visibleName.IncrementalSearchEnabled = true;
            this._visibleName.LeftMargin = 8;
            this._visibleName.ParentColumn = this.treeColumn1;
            this._visibleName.Trimming = System.Drawing.StringTrimming.EllipsisCharacter;
            this._visibleName.UseCompatibleTextRendering = true;
            // 
            // _returnType
            // 
            this._returnType.DataPropertyName = "ReturnType";
            this._returnType.IncrementalSearchEnabled = true;
            this._returnType.LeftMargin = 3;
            this._returnType.ParentColumn = this.treeColumn3;
            // 
            // _parameters
            // 
            this._parameters.DataPropertyName = "Parameters";
            this._parameters.IncrementalSearchEnabled = true;
            this._parameters.LeftMargin = 3;
            this._parameters.ParentColumn = this.treeColumn4;
            // 
            // _exceptionString
            // 
            this._exceptionString.DataPropertyName = "ExceptionString";
            this._exceptionString.IncrementalSearchEnabled = true;
            this._exceptionString.LeftMargin = 3;
            this._exceptionString.ParentColumn = this.treeColumn5;
            // 
            // _materialParametersGrid
            // 
            this._materialParametersGrid.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialParametersGrid.HelpVisible = false;
            this._materialParametersGrid.Location = new System.Drawing.Point(0, 0);
            this._materialParametersGrid.Name = "_materialParametersGrid";
            this._materialParametersGrid.Size = new System.Drawing.Size(288, 112);
            this._materialParametersGrid.TabIndex = 2;
            this._materialParametersGrid.ToolbarVisible = false;
            this._materialParametersGrid.PropertyValueChanged += new System.Windows.Forms.PropertyValueChangedEventHandler(this._materialParametersGrid_PropertyValueChanged);
            // 
            // _ribbon
            // 
            this._ribbon.Location = new System.Drawing.Point(0, 0);
            this._ribbon.Minimized = false;
            this._ribbon.Name = "_ribbon";
            this._ribbon.ResourceName = "NodeEditor.RibbonMarkup.ribbon";
            this._ribbon.ShortcutTableResourceName = null;
            this._ribbon.Size = new System.Drawing.Size(1300, 111);
            this._ribbon.TabIndex = 1;
            // 
            // _materialPropertiesGrid
            // 
            this._materialPropertiesGrid.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialPropertiesGrid.HelpVisible = false;
            this._materialPropertiesGrid.Location = new System.Drawing.Point(0, 0);
            this._materialPropertiesGrid.Name = "_materialPropertiesGrid";
            this._materialPropertiesGrid.Size = new System.Drawing.Size(288, 112);
            this._materialPropertiesGrid.TabIndex = 0;
            this._materialPropertiesGrid.ToolbarVisible = false;
            // 
            // parameterBoxMenu
            // 
            this.parameterBoxMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.addParameterToolStripMenuItem});
            this.parameterBoxMenu.Name = "parameterBoxMenu";
            this.parameterBoxMenu.Size = new System.Drawing.Size(154, 26);
            // 
            // addParameterToolStripMenuItem
            // 
            this.addParameterToolStripMenuItem.Name = "addParameterToolStripMenuItem";
            this.addParameterToolStripMenuItem.Size = new System.Drawing.Size(153, 22);
            this.addParameterToolStripMenuItem.Text = "Add Parameter";
            this.addParameterToolStripMenuItem.Click += new System.EventHandler(this.addParameterToolStripMenuItem_Click);
            // 
            // largePreviewToolStripMenuItem
            // 
            this.largePreviewToolStripMenuItem.Name = "largePreviewToolStripMenuItem";
            this.largePreviewToolStripMenuItem.Size = new System.Drawing.Size(186, 22);
            this.largePreviewToolStripMenuItem.Text = "Large Preview";
            this.largePreviewToolStripMenuItem.Click += new System.EventHandler(this.largePreviewToolStripMenuItem_Click);
            // 
            // ExampleForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.ClientSize = new System.Drawing.Size(1300, 550);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this._ribbon);
            this.Name = "ExampleForm";
            this.Text = "Shader Construction";
            this.nodeMenu.ResumeLayout(false);
            this.panel1.ResumeLayout(false);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.panel2.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.parameterBoxMenu.ResumeLayout(false);
            this.ResumeLayout(false);

		}

		#endregion

        private HyperGraph.GraphControl graphControl;
        private Aga.Controls.Tree.NodeControls.NodeStateIcon _icon;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _visibleName;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _returnType;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _parameters;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _exceptionString;
        private System.Windows.Forms.ContextMenuStrip nodeMenu;
        private System.Windows.Forms.ToolStripMenuItem showPreviewShaderItem;
        private System.Windows.Forms.Panel panel1;
        private RibbonLib.Ribbon _ribbon;
        private Aga.Controls.Tree.TreeViewAdv _fragmentTree;
        private Aga.Controls.Tree.TreeColumn treeColumn1;
        private Aga.Controls.Tree.TreeColumn treeColumn3;
        private Aga.Controls.Tree.TreeColumn treeColumn4;
        private Aga.Controls.Tree.TreeColumn treeColumn5;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.PropertyGrid _materialParametersGrid;
        private System.Windows.Forms.PropertyGrid _materialPropertiesGrid;
        private System.Windows.Forms.ToolStripMenuItem refreshToolStripMenuItem;
        private System.Windows.Forms.ContextMenuStrip parameterBoxMenu;
        private System.Windows.Forms.ToolStripMenuItem addParameterToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem largePreviewToolStripMenuItem;
	}
}

