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
            this._compareWindow = new FileCompare();
            this._okButton = new System.Windows.Forms.Button();
            this._cancelButton = new System.Windows.Forms.Button();
            this._label = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this._nodeCheckBox = new Aga.Controls.Tree.NodeControls.NodeCheckBox();
            this.SuspendLayout();
            // 
            // _assetList
            // 
            this._assetList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._assetList.BackColor = System.Drawing.SystemColors.Window;
            this._assetList.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._assetList.DefaultToolTipProvider = null;
            this._assetList.DragDropMarkColor = System.Drawing.Color.Black;
            this._assetList.LineColor = System.Drawing.SystemColors.ControlDark;
            this._assetList.Location = new System.Drawing.Point(12, 12);
            this._assetList.NodeControls.Add(this._nodeCheckBox);
            this._assetList.NodeControls.Add(this._label);
            this._assetList.Model = null;
            this._assetList.SelectedNode = null;
            this._assetList.Size = new System.Drawing.Size(628, 161);
            this._assetList.TabIndex = 0;
            // 
            // _label
            // 
            this._label.DataPropertyName = "Label";
            this._label.IncrementalSearchEnabled = true;
            this._label.LeftMargin = 3;
            // 
            // _nodeCheckBox
            // 
            this._nodeCheckBox.DataPropertyName = "Enabled";
            this._nodeCheckBox.EditEnabled = true;
            // 
            // _compareWindow
            // 
            this._compareWindow.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._compareWindow.Location = new System.Drawing.Point(12, 179);
            this._compareWindow.Size = new System.Drawing.Size(628, 233);
            this._compareWindow.TabIndex = 1;
            // 
            // _okButton
            // 
            this._okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._okButton.Location = new System.Drawing.Point(523, 418);
            this._okButton.Name = "_okButton";
            this._okButton.Size = new System.Drawing.Size(117, 23);
            this._okButton.TabIndex = 2;
            this._okButton.Text = "OK";
            this._okButton.UseVisualStyleBackColor = true;
            // 
            // _cancelButton
            // 
            this._cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this._cancelButton.Location = new System.Drawing.Point(400, 418);
            this._cancelButton.Name = "_cancelButton";
            this._cancelButton.Size = new System.Drawing.Size(117, 23);
            this._cancelButton.TabIndex = 3;
            this._cancelButton.Text = "Cancel";
            this._cancelButton.UseVisualStyleBackColor = true;
            // 
            // ExportPreviewDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(652, 448);
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
    }
}