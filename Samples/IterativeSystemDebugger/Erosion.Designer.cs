// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace IterativeSystemDebugger
{
    using LayerControlType = ControlsLibrary.LayerControl;

    partial class Erosion
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this._previewWindow = new LayerControlType();
            this._systemSettings = new System.Windows.Forms.PropertyGrid();
            this._previewSettings = new System.Windows.Forms.PropertyGrid();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this._previewWindow);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.splitContainer2);
            this.splitContainer1.Size = new System.Drawing.Size(1070, 680);
            this.splitContainer1.SplitterDistance = 707;
            this.splitContainer1.TabIndex = 0;
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Name = "splitContainer2";
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this._previewSettings);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this._systemSettings);
            this.splitContainer2.Size = new System.Drawing.Size(359, 680);
            this.splitContainer2.SplitterDistance = 187;
            this.splitContainer2.TabIndex = 0;
            // 
            // _previewWindow
            // 
            this._previewWindow.Dock = System.Windows.Forms.DockStyle.Fill;
            this._previewWindow.Location = new System.Drawing.Point(0, 0);
            this._previewWindow.Name = "_previewWindow";
            this._previewWindow.Size = new System.Drawing.Size(707, 680);
            this._previewWindow.TabIndex = 0;
            this._previewWindow.Text = "button1";
            // 
            // _systemSettings
            // 
            this._systemSettings.Dock = System.Windows.Forms.DockStyle.Fill;
            this._systemSettings.Location = new System.Drawing.Point(0, 0);
            this._systemSettings.Name = "_systemSettings";
            this._systemSettings.Size = new System.Drawing.Size(359, 489);
            this._systemSettings.TabIndex = 0;
            // 
            // _previewSettings
            // 
            this._previewSettings.Dock = System.Windows.Forms.DockStyle.Fill;
            this._previewSettings.Location = new System.Drawing.Point(0, 0);
            this._previewSettings.Name = "_previewSettings";
            this._previewSettings.Size = new System.Drawing.Size(359, 187);
            this._previewSettings.TabIndex = 0;
            // 
            // Erosion
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1070, 680);
            this.Controls.Add(this.splitContainer1);
            this.Name = "Erosion";
            this.Text = "Erosion";
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private LayerControlType _previewWindow;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.PropertyGrid _previewSettings;
        private System.Windows.Forms.PropertyGrid _systemSettings;
    }
}