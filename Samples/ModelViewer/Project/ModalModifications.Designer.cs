namespace ModelViewer
{
    partial class ModalModifications
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
            this._modifiedAssets = new Aga.Controls.Tree.TreeViewAdv();
            this._label = new Aga.Controls.Tree.NodeControls.NodeTextBox();
            this.SuspendLayout();
            // 
            // _modifiedAssets
            // 
            this._modifiedAssets.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._modifiedAssets.BackColor = System.Drawing.SystemColors.Window;
            this._modifiedAssets.ColumnFont = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this._modifiedAssets.DefaultToolTipProvider = null;
            this._modifiedAssets.DragDropMarkColor = System.Drawing.Color.Black;
            this._modifiedAssets.LineColor = System.Drawing.SystemColors.ControlDark;
            this._modifiedAssets.Location = new System.Drawing.Point(12, 12);
            this._modifiedAssets.Model = null;
            this._modifiedAssets.Name = "_modifiedAssets";
            this._modifiedAssets.NodeControls.Add(this._label);
            this._modifiedAssets.SelectedNode = null;
            this._modifiedAssets.Size = new System.Drawing.Size(368, 687);
            this._modifiedAssets.TabIndex = 0;
            this._modifiedAssets.Text = "treeViewAdv1";
            // 
            // _label
            // 
            this._label.DataPropertyName = "Label";
            this._label.IncrementalSearchEnabled = true;
            this._label.LeftMargin = 3;
            // 
            // ModalModifications
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(392, 738);
            this.Controls.Add(this._modifiedAssets);
            this.Name = "ModalModifications";
            this.Text = "ModelModifications";
            this.ResumeLayout(false);

        }

        #endregion

        private Aga.Controls.Tree.TreeViewAdv _modifiedAssets;
        private Aga.Controls.Tree.NodeControls.NodeTextBox _label;
    }
}