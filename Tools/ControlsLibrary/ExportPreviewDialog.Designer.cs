namespace ControlsLibrary
{
    partial class ExportPreviewDialog
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
            this._assetList = new Aga.Controls.Tree.TreeViewAdv();
            this._checkColumn = new Aga.Controls.Tree.TreeColumn();
            this._labelColumn = new Aga.Controls.Tree.TreeColumn();
            this._nodeCheckBox = new Aga.Controls.Tree.NodeControls.NodeCheckBox();
            this._label = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._okButton = new System.Windows.Forms.Button();
            this._cancelButton = new System.Windows.Forms.Button();
            this._compareWindow = new ControlsLibrary.FileCompare();
            this.SuspendLayout();
            // 
            // _assetList
            // 
            this._assetList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._assetList.BackColor = System.Drawing.SystemColors.Window;
            this._assetList.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._assetList.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._assetList.Columns.Add(this._checkColumn);
            this._assetList.Columns.Add(this._labelColumn);
            this._assetList.DefaultToolTipProvider = null;
            this._assetList.DragDropMarkColor = System.Drawing.Color.Black;
            this._assetList.FullRowSelect = true;
            this._assetList.GridLineStyle = ((Aga.Controls.Tree.GridLineStyle)((Aga.Controls.Tree.GridLineStyle.Horizontal | Aga.Controls.Tree.GridLineStyle.Vertical)));
            this._assetList.LineColor = System.Drawing.SystemColors.ControlDark;
            this._assetList.Location = new System.Drawing.Point(12, 12);
            this._assetList.Model = null;
            this._assetList.Name = "_assetList";
            this._assetList.NodeControls.Add(this._nodeCheckBox);
            this._assetList.NodeControls.Add(this._label);
            this._assetList.SelectedNode = null;
            this._assetList.Size = new System.Drawing.Size(267, 230);
            this._assetList.TabIndex = 0;
            this._assetList.UseColumns = true;
            this._assetList.SelectionChanged += new System.EventHandler(this.OnSelectionChange);
            this._assetList.Resize += new System.EventHandler(this.OnTreeResize);
            // 
            // _checkColumn
            // 
            this._checkColumn.Header = "Enabled";
            this._checkColumn.SortOrder = System.Windows.Forms.SortOrder.None;
            this._checkColumn.TooltipText = null;
            this._checkColumn.Width = 75;
            // 
            // _labelColumn
            // 
            this._labelColumn.Header = "Target file";
            this._labelColumn.SortOrder = System.Windows.Forms.SortOrder.None;
            this._labelColumn.TooltipText = null;
            // 
            // _nodeCheckBox
            // 
            this._nodeCheckBox.DataPropertyName = "Enabled";
            this._nodeCheckBox.EditEnabled = true;
            this._nodeCheckBox.LeftMargin = 0;
            this._nodeCheckBox.ParentColumn = this._checkColumn;
            // 
            // _label
            // 
            this._label.DataPropertyName = "Label";
            this._label.IncrementalSearchEnabled = true;
            this._label.LeftMargin = 3;
            this._label.ParentColumn = this._labelColumn;
            // 
            // _okButton
            // 
            this._okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._okButton.Location = new System.Drawing.Point(782, 219);
            this._okButton.Name = "_okButton";
            this._okButton.Size = new System.Drawing.Size(117, 23);
            this._okButton.TabIndex = 2;
            this._okButton.Text = "Do Export";
            this._okButton.UseVisualStyleBackColor = true;
            // 
            // _cancelButton
            // 
            this._cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this._cancelButton.Location = new System.Drawing.Point(659, 219);
            this._cancelButton.Name = "_cancelButton";
            this._cancelButton.Size = new System.Drawing.Size(117, 23);
            this._cancelButton.TabIndex = 3;
            this._cancelButton.Text = "Cancel";
            this._cancelButton.UseVisualStyleBackColor = true;
            // 
            // _compareWindow
            // 
            this._compareWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._compareWindow.Location = new System.Drawing.Point(285, 12);
            this._compareWindow.Margin = new System.Windows.Forms.Padding(0);
            this._compareWindow.Name = "_compareWindow";
            this._compareWindow.Size = new System.Drawing.Size(614, 201);
            this._compareWindow.TabIndex = 1;
            // 
            // ExportPreviewDialog
            // 
            this.AcceptButton = this._okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this._cancelButton;
            this.ClientSize = new System.Drawing.Size(908, 254);
            this.Controls.Add(this._cancelButton);
            this.Controls.Add(this._okButton);
            this.Controls.Add(this._compareWindow);
            this.Controls.Add(this._assetList);
            this.Name = "ExportPreviewDialog";
            this.Text = "Export Preview";
            this.ResumeLayout(false);

        }

        #endregion

        private Aga.Controls.Tree.TreeViewAdv _assetList;
        private FileCompare _compareWindow;
        private System.Windows.Forms.Button _okButton;
        private System.Windows.Forms.Button _cancelButton;
        private Aga.Controls.Tree.NodeControls.NodeCheckBox _nodeCheckBox;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _label;
        private Aga.Controls.Tree.TreeColumn _checkColumn;
        private Aga.Controls.Tree.TreeColumn _labelColumn;
    }
}