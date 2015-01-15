namespace HyperGraph
{
	partial class TextEditForm
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
			this.TextTextBox = new System.Windows.Forms.TextBox();
			this.CancelButtonControl = new System.Windows.Forms.Button();
			this.OKButtonControl = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// TextTextBox
			// 
			this.TextTextBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.TextTextBox.Location = new System.Drawing.Point(12, 12);
			this.TextTextBox.Name = "TextTextBox";
			this.TextTextBox.Size = new System.Drawing.Size(258, 20);
			this.TextTextBox.TabIndex = 1;
			// 
			// CancelButtonControl
			// 
			this.CancelButtonControl.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.CancelButtonControl.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelButtonControl.Location = new System.Drawing.Point(114, 38);
			this.CancelButtonControl.Name = "CancelButtonControl";
			this.CancelButtonControl.Size = new System.Drawing.Size(75, 23);
			this.CancelButtonControl.TabIndex = 2;
			this.CancelButtonControl.Text = "&Cancel";
			this.CancelButtonControl.UseVisualStyleBackColor = true;
			// 
			// OKButtonControl
			// 
			this.OKButtonControl.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.OKButtonControl.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.OKButtonControl.Location = new System.Drawing.Point(195, 37);
			this.OKButtonControl.Name = "OKButtonControl";
			this.OKButtonControl.Size = new System.Drawing.Size(75, 23);
			this.OKButtonControl.TabIndex = 3;
			this.OKButtonControl.Text = "&Ok";
			this.OKButtonControl.UseVisualStyleBackColor = true;
			// 
			// TextEditForm
			// 
			this.AcceptButton = this.OKButtonControl;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(282, 72);
			this.ControlBox = false;
			this.Controls.Add(this.OKButtonControl);
			this.Controls.Add(this.CancelButtonControl);
			this.Controls.Add(this.TextTextBox);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
			this.MaximumSize = new System.Drawing.Size(600, 106);
			this.MinimumSize = new System.Drawing.Size(200, 106);
			this.Name = "TextEditForm";
			this.ShowIcon = false;
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Show;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "TextEditForm";
			this.TopMost = true;
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox TextTextBox;
		private System.Windows.Forms.Button CancelButtonControl;
		private System.Windows.Forms.Button OKButtonControl;
	}
}