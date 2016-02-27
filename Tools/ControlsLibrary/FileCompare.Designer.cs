namespace ControlsLibrary
{
    partial class FileCompare
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
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this._originalVersion = new System.Windows.Forms.RichTextBox();
            this._newVersion = new System.Windows.Forms.RichTextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Margin = new System.Windows.Forms.Padding(0);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this._originalVersion);
            this.splitContainer1.Panel1.Controls.Add(this.label1);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.label2);
            this.splitContainer1.Panel2.Controls.Add(this._newVersion);
            this.splitContainer1.Size = new System.Drawing.Size(1532, 556);
            this.splitContainer1.SplitterDistance = 743;
            this.splitContainer1.TabIndex = 0;
            // 
            // _originalVersion
            // 
            this._originalVersion.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._originalVersion.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._originalVersion.Location = new System.Drawing.Point(0, 30);
            this._originalVersion.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._originalVersion.Name = "_originalVersion";
            this._originalVersion.ReadOnly = true;
            this._originalVersion.Size = new System.Drawing.Size(743, 526);
            this._originalVersion.TabIndex = 0;
            this._originalVersion.Text = "";
            this._originalVersion.WordWrap = false;
            // 
            // _newVersion
            // 
            this._newVersion.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._newVersion.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._newVersion.Location = new System.Drawing.Point(0, 30);
            this._newVersion.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._newVersion.Name = "_newVersion";
            this._newVersion.ReadOnly = true;
            this._newVersion.Size = new System.Drawing.Size(785, 526);
            this._newVersion.TabIndex = 0;
            this._newVersion.Text = "";
            this._newVersion.WordWrap = false;
            // 
            // label1
            // 
            this.label1.Dock = System.Windows.Forms.DockStyle.Top;
            this.label1.Location = new System.Drawing.Point(0, 0);
            this.label1.Margin = new System.Windows.Forms.Padding(7, 0, 7, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(743, 24);
            this.label1.TabIndex = 1;
            this.label1.Text = "Original Version";
            this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // label2
            // 
            this.label2.Dock = System.Windows.Forms.DockStyle.Top;
            this.label2.Location = new System.Drawing.Point(0, 0);
            this.label2.Margin = new System.Windows.Forms.Padding(7, 0, 7, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(785, 24);
            this.label2.TabIndex = 2;
            this.label2.Text = "New Version";
            this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // FileCompare
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.Margin = new System.Windows.Forms.Padding(0);
            this.Name = "FileCompare";
            this.Size = new System.Drawing.Size(1532, 556);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.RichTextBox _originalVersion;
        private System.Windows.Forms.RichTextBox _newVersion;
    }
}
