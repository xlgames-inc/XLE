// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibraryExt.ModelView
{
    partial class CtrlStrip
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
            this._selectModel = new System.Windows.Forms.Button();
            this._resetCam = new System.Windows.Forms.Button();
            this._colByMaterial = new System.Windows.Forms.ComboBox();
            this._displayMode = new System.Windows.Forms.ComboBox();
            this._skeletonMode = new System.Windows.Forms.ComboBox();
            this.SuspendLayout();
            // 
            // _selectModel
            // 
            this._selectModel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._selectModel.Location = new System.Drawing.Point(4, 4);
            this._selectModel.Name = "_selectModel";
            this._selectModel.Size = new System.Drawing.Size(223, 23);
            this._selectModel.TabIndex = 0;
            this._selectModel.Text = "Select Model";
            this._selectModel.UseVisualStyleBackColor = true;
            this._selectModel.Click += new System.EventHandler(this.SelectModel);
            // 
            // _resetCam
            // 
            this._resetCam.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._resetCam.Location = new System.Drawing.Point(580, 4);
            this._resetCam.Name = "_resetCam";
            this._resetCam.Size = new System.Drawing.Size(124, 21);
            this._resetCam.TabIndex = 1;
            this._resetCam.Text = "Reset Cam";
            this._resetCam.UseVisualStyleBackColor = true;
            this._resetCam.Click += new System.EventHandler(this.ResetCamClick);
            // 
            // _colByMaterial
            // 
            this._colByMaterial.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._colByMaterial.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._colByMaterial.FormattingEnabled = true;
            this._colByMaterial.Location = new System.Drawing.Point(234, 4);
            this._colByMaterial.Name = "_colByMaterial";
            this._colByMaterial.Size = new System.Drawing.Size(86, 21);
            this._colByMaterial.TabIndex = 2;
            this._colByMaterial.SelectedIndexChanged += new System.EventHandler(this.SelectColorByMaterial);
            // 
            // _displayMode
            // 
            this._displayMode.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._displayMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._displayMode.FormattingEnabled = true;
            this._displayMode.Location = new System.Drawing.Point(326, 4);
            this._displayMode.Name = "_displayMode";
            this._displayMode.Size = new System.Drawing.Size(121, 21);
            this._displayMode.TabIndex = 3;
            this._displayMode.SelectedIndexChanged += new System.EventHandler(this.SelectDisplayMode);
            // 
            // _skeletonMode
            // 
            this._skeletonMode.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._skeletonMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._skeletonMode.FormattingEnabled = true;
            this._skeletonMode.Location = new System.Drawing.Point(453, 4);
            this._skeletonMode.Name = "_skeletonMode";
            this._skeletonMode.Size = new System.Drawing.Size(121, 21);
            this._skeletonMode.TabIndex = 4;
            this._skeletonMode.SelectedIndexChanged += new System.EventHandler(this.SelectSkeletonMode);
            // 
            // CtrlStrip
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._skeletonMode);
            this.Controls.Add(this._displayMode);
            this.Controls.Add(this._colByMaterial);
            this.Controls.Add(this._resetCam);
            this.Controls.Add(this._selectModel);
            this.Name = "CtrlStrip";
            this.Size = new System.Drawing.Size(707, 32);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button _selectModel;
        private System.Windows.Forms.Button _resetCam;
        private System.Windows.Forms.ComboBox _colByMaterial;
        private System.Windows.Forms.ComboBox _displayMode;
        private System.Windows.Forms.ComboBox _skeletonMode;
    }
}
