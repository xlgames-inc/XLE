namespace ControlsLibraryExt.SceneSelector
{
    partial class PreviewSceneSelector
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.treeView1 = new Aga.Controls.Tree.TreeViewAdv();
            this._labelColumn = new Aga.Controls.Tree.TreeColumn();
            this._label = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this.SuspendLayout();
            // 
            // treeView1
            // 
            this.treeView1.AutoRowHeight = true;
            this.treeView1.BackColor = System.Drawing.SystemColors.Window;
            this.treeView1.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.treeView1.Columns.Add(this._labelColumn);
            this.treeView1.DefaultToolTipProvider = null;
            this.treeView1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.treeView1.DragDropMarkColor = System.Drawing.Color.Black;
            this.treeView1.LineColor = System.Drawing.SystemColors.ControlDark;
            this.treeView1.Location = new System.Drawing.Point(0, 0);
            this.treeView1.Model = null;
            this.treeView1.Name = "treeView1";
            this.treeView1.NodeControls.Add(this._label);
            this.treeView1.SelectedNode = null;
            this.treeView1.Size = new System.Drawing.Size(723, 321);
            this.treeView1.TabIndex = 0;
            this.treeView1.UseColumns = true;
            // 
            // _labelColumn
            // 
            this._labelColumn.Header = "Label";
            this._labelColumn.SortOrder = System.Windows.Forms.SortOrder.None;
            this._labelColumn.TooltipText = null;
            this._labelColumn.Width = 200;
            // 
            // _label
            // 
            this._label.DataPropertyName = "Label";
            this._label.IncrementalSearchEnabled = true;
            this._label.LeftMargin = 3;
            this._label.ParentColumn = this._labelColumn;
            // 
            // PreviewSceneSelector
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.treeView1);
            this.Name = "PreviewSceneSelector";
            this.Size = new System.Drawing.Size(723, 321);
            this.ResumeLayout(false);

        }

        #endregion

        private Aga.Controls.Tree.TreeViewAdv treeView1;
        private Aga.Controls.Tree.TreeColumn _labelColumn;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _label;
    }
}
