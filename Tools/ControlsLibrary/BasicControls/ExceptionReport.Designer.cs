namespace ControlsLibrary.BasicControls
{
    partial class ExceptionReport
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
            this.label1 = new System.Windows.Forms.Label();
            this._while = new System.Windows.Forms.TextBox();
            this._message = new System.Windows.Forms.TextBox();
            this._callstack = new System.Windows.Forms.TextBox();
            this._okButton = new System.Windows.Forms.Button();
            this._type = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(20, 22);
            this.label1.Margin = new System.Windows.Forms.Padding(5, 0, 5, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(80, 24);
            this.label1.TabIndex = 0;
            this.label1.Text = "During:";
            // 
            // _while
            // 
            this._while.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._while.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._while.Location = new System.Drawing.Point(106, 18);
            this._while.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this._while.Name = "_while";
            this._while.ReadOnly = true;
            this._while.Size = new System.Drawing.Size(572, 35);
            this._while.TabIndex = 1;
            this._while.WordWrap = false;
            // 
            // _message
            // 
            this._message.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._message.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._message.Location = new System.Drawing.Point(21, 96);
            this._message.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this._message.Multiline = true;
            this._message.Name = "_message";
            this._message.ReadOnly = true;
            this._message.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._message.Size = new System.Drawing.Size(831, 218);
            this._message.TabIndex = 3;
            // 
            // _callstack
            // 
            this._callstack.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._callstack.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._callstack.Location = new System.Drawing.Point(20, 324);
            this._callstack.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this._callstack.Multiline = true;
            this._callstack.Name = "_callstack";
            this._callstack.ReadOnly = true;
            this._callstack.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this._callstack.Size = new System.Drawing.Size(831, 192);
            this._callstack.TabIndex = 4;
            // 
            // _okButton
            // 
            this._okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this._okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._okButton.Location = new System.Drawing.Point(689, 18);
            this._okButton.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this._okButton.Name = "_okButton";
            this._okButton.Size = new System.Drawing.Size(164, 33);
            this._okButton.TabIndex = 0;
            this._okButton.Text = "Ok";
            this._okButton.UseVisualStyleBackColor = true;
            // 
            // _type
            // 
            this._type.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._type.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this._type.Location = new System.Drawing.Point(21, 60);
            this._type.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this._type.Name = "_type";
            this._type.ReadOnly = true;
            this._type.Size = new System.Drawing.Size(829, 35);
            this._type.TabIndex = 2;
            // 
            // ExceptionReport
            // 
            this.AcceptButton = this._okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this._okButton;
            this.ClientSize = new System.Drawing.Size(873, 536);
            this.Controls.Add(this._type);
            this.Controls.Add(this._okButton);
            this.Controls.Add(this._callstack);
            this.Controls.Add(this._message);
            this.Controls.Add(this._while);
            this.Controls.Add(this.label1);
            this.Margin = new System.Windows.Forms.Padding(5, 4, 5, 4);
            this.Name = "ExceptionReport";
            this.Text = "Exception Report";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox _while;
        private System.Windows.Forms.TextBox _message;
        private System.Windows.Forms.TextBox _callstack;
        private System.Windows.Forms.Button _okButton;
        private System.Windows.Forms.TextBox _type;
    }
}