namespace NodeEditor
{
    partial class LargePreview
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
            this.SuspendLayout();
            // 
            // LargePreview
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1218, 662);
            this.Name = "LargePreview";
            this.Text = "LargePreview";
            this.Activated += new System.EventHandler(this.LargePreview_Activated);
            this.Paint += new System.Windows.Forms.PaintEventHandler(this.LargePreview_Paint);
            this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.LargePreview_MouseDown);
            this.MouseMove += new System.Windows.Forms.MouseEventHandler(this.LargePreview_MouseMove);
            this.MouseUp += new System.Windows.Forms.MouseEventHandler(this.LargePreview_MouseUp);
            this.Resize += new System.EventHandler(this.LargePreview_Resize);
            this.ResumeLayout(false);

        }

        #endregion
    }
}