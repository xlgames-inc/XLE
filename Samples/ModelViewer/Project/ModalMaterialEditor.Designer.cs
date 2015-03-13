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
            this.hierchicalMaterialControl1 = new ModelViewer.HierchicalMaterialControl();
            this.SuspendLayout();
            // 
            // hierchicalMaterialControl1
            // 
            this.hierchicalMaterialControl1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.hierchicalMaterialControl1.Location = new System.Drawing.Point(0, 0);
            this.hierchicalMaterialControl1.Name = "hierchicalMaterialControl1";
            this.hierchicalMaterialControl1.Size = new System.Drawing.Size(612, 503);
            this.hierchicalMaterialControl1.TabIndex = 0;
            // 
            // ModalMaterialEditor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(612, 503);
            this.Controls.Add(this.hierchicalMaterialControl1);
            this.Name = "ModalMaterialEditor";
            this.Text = "ModalMaterialEditor";
            this.ResumeLayout(false);

        }

        #endregion

        private HierchicalMaterialControl hierchicalMaterialControl1;
    }
}