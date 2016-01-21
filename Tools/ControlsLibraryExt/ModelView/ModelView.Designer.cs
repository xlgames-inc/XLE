// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibraryExt.ModelView
{
    partial class ModelView
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._ctrls = new CtrlStrip();
            this._view = new ControlsLibrary.LayerControl();
            this.SuspendLayout();
            // 
            // _ctrls
            // 
            this._ctrls.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._ctrls.Location = new System.Drawing.Point(0, 512);
            this._ctrls.Size = new System.Drawing.Size(677, 32);
            this._ctrls.TabIndex = 0;
            // 
            // _view
            // 
            this._view.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._view.Location = new System.Drawing.Point(0, 0);
            this._view.Size = new System.Drawing.Size(677, 506);
            this._view.TabIndex = 1;
            // 
            // ModelView
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._view);
            this.Controls.Add(this._ctrls);
            this.Margin = new System.Windows.Forms.Padding(0);
            this.Name = "ModelView";
            this.Size = new System.Drawing.Size(677, 544);
            this.ResumeLayout(false);

        }

        #endregion

        private CtrlStrip _ctrls;
        private ControlsLibrary.LayerControl _view;
    }
}
