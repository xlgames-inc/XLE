namespace ControlsLibrary
{
    partial class InvalidAssetDialog
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
                _list.Dispose();
                _list = null;
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
            this._assetList = new System.Windows.Forms.ListBox();
            this._closeButton = new System.Windows.Forms.Button();
            this._errorDetails = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // _assetList
            // 
            this._assetList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this._assetList.FormattingEnabled = true;
            this._assetList.ItemHeight = 24;
            this._assetList.Location = new System.Drawing.Point(16, 22);
            this._assetList.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._assetList.Name = "_assetList";
            this._assetList.Size = new System.Drawing.Size(328, 364);
            this._assetList.TabIndex = 0;
            // 
            // _closeButton
            // 
            this._closeButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this._closeButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this._closeButton.Location = new System.Drawing.Point(16, 426);
            this._closeButton.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._closeButton.Name = "_closeButton";
            this._closeButton.Size = new System.Drawing.Size(333, 46);
            this._closeButton.TabIndex = 1;
            this._closeButton.Text = "Close";
            this._closeButton.UseVisualStyleBackColor = true;
            this._closeButton.Click += new System.EventHandler(this._closeButton_Click);
            // 
            // _errorDetails
            // 
            this._errorDetails.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._errorDetails.Location = new System.Drawing.Point(362, 22);
            this._errorDetails.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this._errorDetails.Multiline = true;
            this._errorDetails.Name = "_errorDetails";
            this._errorDetails.ReadOnly = true;
            this._errorDetails.Size = new System.Drawing.Size(1464, 446);
            this._errorDetails.TabIndex = 2;
            this._errorDetails.WordWrap = false;
            // 
            // InvalidAssetDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this._closeButton;
            this.ClientSize = new System.Drawing.Size(1842, 494);
            this.Controls.Add(this._errorDetails);
            this.Controls.Add(this._closeButton);
            this.Controls.Add(this._assetList);
            this.Margin = new System.Windows.Forms.Padding(7, 6, 7, 6);
            this.Name = "InvalidAssetDialog";
            this.Text = "InvalidAssetForm";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListBox _assetList;
        private System.Windows.Forms.Button _closeButton;
        private System.Windows.Forms.TextBox _errorDetails;
    }
}