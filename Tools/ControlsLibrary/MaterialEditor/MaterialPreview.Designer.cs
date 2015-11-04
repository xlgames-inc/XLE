// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibrary.MaterialEditor
{
    partial class MaterialPreview
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
            if (disposing)
            {
                if (components != null) components.Dispose();
                visSettings.Dispose();
                visLayer.Dispose();
                _preview.Dispose();
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
            this._resetCamera = new System.Windows.Forms.Button();
            this._lightingType = new System.Windows.Forms.ComboBox();
            this._geoType = new System.Windows.Forms.ComboBox();
            this._preview = new LayerControl();
            this._environment = new System.Windows.Forms.ComboBox();
            this.SuspendLayout();
            // 
            // _resetCamera
            // 
            this._resetCamera.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._resetCamera.Location = new System.Drawing.Point(487, 422);
            this._resetCamera.Name = "_resetCamera";
            this._resetCamera.Size = new System.Drawing.Size(85, 21);
            this._resetCamera.TabIndex = 3;
            this._resetCamera.Text = "Reset Camera";
            this._resetCamera.UseVisualStyleBackColor = true;
            this._resetCamera.Click += new System.EventHandler(this._resetCamera_Click);
            // 
            // _lightingType
            // 
            this._lightingType.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._lightingType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._lightingType.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._lightingType.FormattingEnabled = true;
            this._lightingType.Location = new System.Drawing.Point(165, 422);
            this._lightingType.Name = "_lightingType";
            this._lightingType.Size = new System.Drawing.Size(155, 21);
            this._lightingType.TabIndex = 1;
            // 
            // _geoType
            // 
            this._geoType.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._geoType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._geoType.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._geoType.FormattingEnabled = true;
            this._geoType.Location = new System.Drawing.Point(4, 422);
            this._geoType.Name = "_geoType";
            this._geoType.Size = new System.Drawing.Size(155, 21);
            this._geoType.TabIndex = 0;
            // 
            // _preview
            // 
            this._preview.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._preview.Location = new System.Drawing.Point(4, 4);
            this._preview.Name = "_preview";
            this._preview.Size = new System.Drawing.Size(567, 412);
            this._preview.TabIndex = 1;
            // 
            // _environment
            // 
            this._environment.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._environment.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._environment.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._environment.FormattingEnabled = true;
            this._environment.Location = new System.Drawing.Point(326, 422);
            this._environment.Name = "_environment";
            this._environment.Size = new System.Drawing.Size(155, 21);
            this._environment.TabIndex = 4;
            this._environment.Visible = false;
            // 
            // MaterialPreview
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._environment);
            this.Controls.Add(this._resetCamera);
            this.Controls.Add(this._preview);
            this.Controls.Add(this._lightingType);
            this.Controls.Add(this._geoType);
            this.Name = "MaterialPreview";
            this.Size = new System.Drawing.Size(574, 446);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ComboBox _geoType;
        private System.Windows.Forms.ComboBox _lightingType;
        private LayerControl _preview;
        private System.Windows.Forms.Button _resetCamera;
        private System.Windows.Forms.ComboBox _environment;
    }
}
