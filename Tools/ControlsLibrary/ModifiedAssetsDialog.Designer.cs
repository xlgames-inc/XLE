namespace ControlsLibrary
{
    partial class ModifiedAssetsDialog
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
            this._treeColumn1 = new Aga.Controls.Tree.TreeColumn();
            this._treeColumn2 = new Aga.Controls.Tree.TreeColumn();
            this._icon = new Aga.Controls.Tree.NodeControls.NodeStateIcon();
            this._action = new ControlsLibrary.PrettyNodeComboBox();
            this._label = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._compareWindow = new ControlsLibrary.FileCompare();
            this._saveButton = new System.Windows.Forms.Button();
            this._cancelButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // _assetList
            // 
            this._assetList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._assetList.AutoRowHeight = true;
            this._assetList.BackColor = System.Drawing.SystemColors.Window;
            this._assetList.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._assetList.Columns.Add(this._treeColumn1);
            this._assetList.Columns.Add(this._treeColumn2);
            this._assetList.DefaultToolTipProvider = null;
            this._assetList.DragDropMarkColor = System.Drawing.Color.Black;
            this._assetList.LineColor = System.Drawing.SystemColors.ControlDark;
            this._assetList.Location = new System.Drawing.Point(16, 15);
            this._assetList.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._assetList.Model = null;
            this._assetList.Name = "_assetList";
            this._assetList.NodeControls.Add(this._icon);
            this._assetList.NodeControls.Add(this._action);
            this._assetList.NodeControls.Add(this._label);
            this._assetList.SelectedNode = null;
            this._assetList.Size = new System.Drawing.Size(640, 566);
            this._assetList.TabIndex = 2;
            this._assetList.SelectionChanged += new System.EventHandler(this._tree_SelectionChanged);
            // 
            // _treeColumn1
            // 
            this._treeColumn1.Header = "Action";
            this._treeColumn1.SortOrder = System.Windows.Forms.SortOrder.None;
            this._treeColumn1.TooltipText = null;
            this._treeColumn1.Width = 75;
            // 
            // _treeColumn2
            // 
            this._treeColumn2.Header = "Asset";
            this._treeColumn2.SortOrder = System.Windows.Forms.SortOrder.None;
            this._treeColumn2.TooltipText = null;
            // 
            // _icon
            // 
            this._icon.DataPropertyName = "Icon";
            this._icon.LeftMargin = 1;
            this._icon.ParentColumn = this._treeColumn2;
            this._icon.ScaleMode = Aga.Controls.Tree.ImageScaleMode.Clip;
            // 
            // _action
            // 
            this._action.DataPropertyName = "Action";
            this._action.EditEnabled = true;
            this._action.EditOnClick = true;
            this._action.IncrementalSearchEnabled = true;
            this._action.LeftMargin = 3;
            this._action.ParentColumn = this._treeColumn1;
            // 
            // _label
            // 
            this._label.DataPropertyName = "Label";
            this._label.IncrementalSearchEnabled = true;
            this._label.LeftMargin = 3;
            this._label.ParentColumn = this._treeColumn2;
            // 
            // _compareWindow
            // 
            this._compareWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._compareWindow.Location = new System.Drawing.Point(678, 20);
            this._compareWindow.Margin = new System.Windows.Forms.Padding(15, 11, 15, 11);
            this._compareWindow.Name = "_compareWindow";
            this._compareWindow.Size = new System.Drawing.Size(1680, 621);
            this._compareWindow.TabIndex = 3;
            // 
            // _saveButton
            // 
            this._saveButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._saveButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._saveButton.Location = new System.Drawing.Point(287, 593);
            this._saveButton.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._saveButton.Name = "_saveButton";
            this._saveButton.Size = new System.Drawing.Size(369, 48);
            this._saveButton.TabIndex = 0;
            this._saveButton.Text = "Commit";
            this._saveButton.UseVisualStyleBackColor = true;
            this._saveButton.Click += new System.EventHandler(this._saveButton_Click);
            // 
            // _cancelButton
            // 
            this._cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this._cancelButton.Location = new System.Drawing.Point(16, 593);
            this._cancelButton.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._cancelButton.Name = "_cancelButton";
            this._cancelButton.Size = new System.Drawing.Size(257, 48);
            this._cancelButton.TabIndex = 1;
            this._cancelButton.Text = "Cancel";
            this._cancelButton.UseVisualStyleBackColor = true;
            this._cancelButton.Click += new System.EventHandler(this._cancelButton_Click);
            // 
            // ModifiedAssetsDialog
            // 
            this.AcceptButton = this._saveButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this._cancelButton;
            this.ClientSize = new System.Drawing.Size(2374, 656);
            this.Controls.Add(this._cancelButton);
            this.Controls.Add(this._saveButton);
            this.Controls.Add(this._compareWindow);
            this.Controls.Add(this._assetList);
            this.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this.Name = "ModifiedAssetsDialog";
            this.Text = "Save Modified Assets";
            this.ResumeLayout(false);

        }

        #endregion

        private Aga.Controls.Tree.TreeViewAdv _assetList;
        private FileCompare _compareWindow;
        private System.Windows.Forms.Button _saveButton;
        private System.Windows.Forms.Button _cancelButton;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _label;
        private Aga.Controls.Tree.NodeControls.NodeStateIcon _icon;
        private Aga.Controls.Tree.TreeColumn _treeColumn1;
        private Aga.Controls.Tree.TreeColumn _treeColumn2;
        private PrettyNodeComboBox _action;
    }
}