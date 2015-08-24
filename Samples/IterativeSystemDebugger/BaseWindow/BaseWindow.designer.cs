// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace IterativeSystemDebugger
{
    using LayerControlType = ControlsLibrary.LayerControl;
    // using LayerControlType = System.Windows.Forms.Button;

    partial class BaseWindow
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
            this._previewWindow = new ControlsLibrary.LayerControl();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this._previewSettings = new System.Windows.Forms.PropertyGrid();
            this._tickButton = new System.Windows.Forms.Button();
            this._systemSettings = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
            this._autoTick = new System.Windows.Forms.CheckBox();
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
            this.splitContainer1.Size = new System.Drawing.Size(819, 629);
            this.splitContainer1.SplitterDistance = 547;
            this.splitContainer1.TabIndex = 0;
            // 
            // _previewWindow
            // 
            this._previewWindow.Dock = System.Windows.Forms.DockStyle.Fill;
            this._previewWindow.Location = new System.Drawing.Point(0, 0);
            this._previewWindow.Size = new System.Drawing.Size(547, 629);
            this._previewWindow.TabIndex = 0;
            this._previewWindow.Text = "button1";
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(0, 0);
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this._previewSettings);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this._autoTick);
            this.splitContainer2.Panel2.Controls.Add(this._tickButton);
            this.splitContainer2.Panel2.Controls.Add(this._systemSettings);
            this.splitContainer2.Size = new System.Drawing.Size(268, 629);
            this.splitContainer2.SplitterDistance = 174;
            this.splitContainer2.TabIndex = 0;
            // 
            // _previewSettings
            // 
            this._previewSettings.CategoryForeColor = System.Drawing.SystemColors.InactiveCaptionText;
            this._previewSettings.Dock = System.Windows.Forms.DockStyle.Fill;
            this._previewSettings.Location = new System.Drawing.Point(0, 0);
            this._previewSettings.Size = new System.Drawing.Size(268, 174);
            this._previewSettings.TabIndex = 0;
            // 
            // _tickButton
            // 
            this._tickButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._tickButton.Location = new System.Drawing.Point(109, 427);
            this._tickButton.Size = new System.Drawing.Size(156, 21);
            this._tickButton.TabIndex = 1;
            this._tickButton.Text = "Tick";
            this._tickButton.UseVisualStyleBackColor = true;
            this._tickButton.Click += new System.EventHandler(this._tickButton_Click);
            // 
            // _systemSettings
            // 
            this._systemSettings.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._systemSettings.Location = new System.Drawing.Point(0, 0);
            this._systemSettings.Size = new System.Drawing.Size(273, 425);
            this._systemSettings.TabIndex = 0;
            // 
            // _autoTick
            // 
            this._autoTick.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._autoTick.AutoSize = true;
            this._autoTick.Location = new System.Drawing.Point(3, 430);
            this._autoTick.Size = new System.Drawing.Size(72, 17);
            this._autoTick.TabIndex = 2;
            this._autoTick.Text = "Auto Tick";
            this._autoTick.UseVisualStyleBackColor = true;
            this._autoTick.CheckedChanged += new System.EventHandler(this._autoTick_CheckedChanged);
            // 
            // IterativeSystem
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(819, 629);
            this.Controls.Add(this.splitContainer1);
            this.Name = "IterativeSystem";
            this.Text = "IterativeSystem";
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            this.splitContainer2.Panel2.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        protected LayerControlType _previewWindow;
        private System.Windows.Forms.SplitContainer splitContainer2;
        protected System.Windows.Forms.PropertyGrid _previewSettings;
        protected Sce.Atf.Controls.PropertyEditing.PropertyGrid _systemSettings;
        private System.Windows.Forms.Button _tickButton;
        private System.Windows.Forms.CheckBox _autoTick;
    }
}