// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibrary
{
    partial class ProgressDialog
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
            this._bar = new System.Windows.Forms.ProgressBar();
            this._stepLabel = new System.Windows.Forms.Label();
            this._cancelBtn = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // _bar
            // 
            this._bar.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._bar.Location = new System.Drawing.Point(12, 12);
            this._bar.Name = "_bar";
            this._bar.Size = new System.Drawing.Size(626, 23);
            this._bar.TabIndex = 0;
            // 
            // _stepLabel
            // 
            this._stepLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._stepLabel.Location = new System.Drawing.Point(12, 38);
            this._stepLabel.Name = "_stepLabel";
            this._stepLabel.Size = new System.Drawing.Size(508, 22);
            this._stepLabel.TabIndex = 1;
            this._stepLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // _cancelBtn
            // 
            this._cancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._cancelBtn.Location = new System.Drawing.Point(526, 37);
            this._cancelBtn.Name = "_cancelBtn";
            this._cancelBtn.Size = new System.Drawing.Size(111, 23);
            this._cancelBtn.TabIndex = 2;
            this._cancelBtn.Text = "Cancel";
            this._cancelBtn.UseVisualStyleBackColor = true;
            this._cancelBtn.Click += new System.EventHandler(this._cancelBtn_Click);
            // 
            // ProgressDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(650, 69);
            this.Controls.Add(this._cancelBtn);
            this.Controls.Add(this._stepLabel);
            this.Controls.Add(this._bar);
            this.Name = "ProgressDialog";
            this.Text = "ProgressDialog";
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ProgressBar _bar;
        private System.Windows.Forms.Label _stepLabel;
        private System.Windows.Forms.Button _cancelBtn;
    }
}