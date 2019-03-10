// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ModelViewer
{
    partial class ModalMaterialEditor
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
            this._matControls = new ControlsLibrary.MaterialEditor.HierchicalMaterialControl();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this._preview = new ControlsLibrary.MaterialEditor.MaterialPreview();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // _matControls
            // 
            this._matControls.Dock = System.Windows.Forms.DockStyle.Fill;
            this._matControls.Location = new System.Drawing.Point(0, 0);
            this._matControls.Size = new System.Drawing.Size(571, 388);
            this._matControls.TabIndex = 0;
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this._preview);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this._matControls);
            this.splitContainer1.Size = new System.Drawing.Size(1005, 388);
            this.splitContainer1.SplitterDistance = 430;
            this.splitContainer1.TabIndex = 1;
            // 
            // _preview
            // 
            this._preview.Dock = System.Windows.Forms.DockStyle.Fill;
            this._preview.Location = new System.Drawing.Point(0, 0);
            this._preview.Size = new System.Drawing.Size(430, 388);
            this._preview.TabIndex = 0;
            // 
            // ModalMaterialEditor
            // 
            this.ClientSize = new System.Drawing.Size(1005, 388);
            this.Controls.Add(this.splitContainer1);
            this.Name = "ModalMaterialEditor";
            this.Text = "ModalMaterialEditor";
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private ControlsLibrary.MaterialEditor.HierchicalMaterialControl _matControls;
        private ControlsLibrary.MaterialEditor.MaterialPreview _preview;
        // private System.Windows.Forms.Button _matControls;
        // private System.Windows.Forms.Button _preview; 
        private System.Windows.Forms.SplitContainer splitContainer1;
    }
}