// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ModelViewer
{
    // using LayerControlType = System.Windows.Forms.Button;
    using LayerControlType = LayerControl;

    partial class MainForm
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
                if (components != null)
                {
                    components.Dispose();
                }
                if (visSettings != null)
                {
                    visSettings.Dispose();
                }
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
            this.viewerControl = new ModelViewer.LayerControl();
            this.viewSettings = new System.Windows.Forms.PropertyGrid();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.mouseOverDetails = new System.Windows.Forms.PropertyGrid();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // viewerControl
            // 
            this.viewerControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this.viewerControl.Location = new System.Drawing.Point(0, 0);
            this.viewerControl.Name = "viewerControl";
            this.viewerControl.Size = new System.Drawing.Size(459, 379);
            this.viewerControl.TabIndex = 0;
            // 
            // viewSettings
            // 
            this.viewSettings.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.viewSettings.HelpVisible = false;
            this.viewSettings.Location = new System.Drawing.Point(0, 0);
            this.viewSettings.Name = "viewSettings";
            this.viewSettings.Size = new System.Drawing.Size(103, 243);
            this.viewSettings.TabIndex = 0;
            this.viewSettings.ToolbarVisible = false;
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel2;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.viewerControl);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.mouseOverDetails);
            this.splitContainer1.Panel2.Controls.Add(this.viewSettings);
            this.splitContainer1.Size = new System.Drawing.Size(566, 379);
            this.splitContainer1.SplitterDistance = 459;
            this.splitContainer1.TabIndex = 0;
            // 
            // mouseOverDetails
            // 
            this.mouseOverDetails.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.mouseOverDetails.HelpVisible = false;
            this.mouseOverDetails.Location = new System.Drawing.Point(0, 249);
            this.mouseOverDetails.Name = "mouseOverDetails";
            this.mouseOverDetails.Size = new System.Drawing.Size(103, 130);
            this.mouseOverDetails.TabIndex = 1;
            this.mouseOverDetails.ToolbarVisible = false;
            // 
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(566, 379);
            this.Controls.Add(this.splitContainer1);
            this.Name = "MainForm";
            this.Text = "MainForm";
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private LayerControlType viewerControl;
        private System.Windows.Forms.PropertyGrid viewSettings;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.PropertyGrid mouseOverDetails;
    }
}