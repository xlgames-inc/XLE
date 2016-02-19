namespace LevelEditorXLE.Terrain
{
    partial class TerrainContextControls
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this._coverageLayer = new System.Windows.Forms.ComboBox();
            this._showLockedArea = new System.Windows.Forms.CheckBox();
            this._lockedAreaLabel = new System.Windows.Forms.Label();
            this._saveButton = new System.Windows.Forms.Button();
            this._abandonButton = new System.Windows.Forms.Button();
            this._historyBox = new System.Windows.Forms.ListBox();
            this._showCoverage = new System.Windows.Forms.CheckBox();
            this.SuspendLayout();
            // 
            // _coverageLayer
            // 
            this._coverageLayer.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._coverageLayer.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._coverageLayer.FormattingEnabled = true;
            this._coverageLayer.Location = new System.Drawing.Point(5, 5);
            this._coverageLayer.Margin = new System.Windows.Forms.Padding(4);
            this._coverageLayer.Name = "_coverageLayer";
            this._coverageLayer.Size = new System.Drawing.Size(340, 24);
            this._coverageLayer.TabIndex = 0;
            this._coverageLayer.SelectedIndexChanged += new System.EventHandler(this._coverageLayer_SelectedIndexChanged);
            // 
            // _showLockedArea
            // 
            this._showLockedArea.AutoSize = true;
            this._showLockedArea.Location = new System.Drawing.Point(5, 38);
            this._showLockedArea.Margin = new System.Windows.Forms.Padding(4);
            this._showLockedArea.Name = "_showLockedArea";
            this._showLockedArea.Size = new System.Drawing.Size(156, 21);
            this._showLockedArea.TabIndex = 1;
            this._showLockedArea.Text = "Show Locked Area :";
            this._showLockedArea.UseVisualStyleBackColor = true;
            this._showLockedArea.CheckedChanged += new System.EventHandler(this._showLockedArea_CheckedChanged);
            // 
            // _lockedAreaLabel
            // 
            this._lockedAreaLabel.AutoSize = true;
            this._lockedAreaLabel.Location = new System.Drawing.Point(177, 39);
            this._lockedAreaLabel.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this._lockedAreaLabel.Name = "_lockedAreaLabel";
            this._lockedAreaLabel.Size = new System.Drawing.Size(0, 17);
            this._lockedAreaLabel.TabIndex = 2;
            // 
            // _saveButton
            // 
            this._saveButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._saveButton.Location = new System.Drawing.Point(5, 95);
            this._saveButton.Margin = new System.Windows.Forms.Padding(4);
            this._saveButton.Name = "_saveButton";
            this._saveButton.Size = new System.Drawing.Size(136, 28);
            this._saveButton.TabIndex = 3;
            this._saveButton.Text = "Save";
            this._saveButton.UseVisualStyleBackColor = true;
            this._saveButton.Click += new System.EventHandler(this._saveButton_Click);
            // 
            // _abandonButton
            // 
            this._abandonButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this._abandonButton.Location = new System.Drawing.Point(148, 95);
            this._abandonButton.Margin = new System.Windows.Forms.Padding(4);
            this._abandonButton.Name = "_abandonButton";
            this._abandonButton.Size = new System.Drawing.Size(197, 28);
            this._abandonButton.TabIndex = 4;
            this._abandonButton.Text = "Abandon";
            this._abandonButton.UseVisualStyleBackColor = true;
            this._abandonButton.Click += new System.EventHandler(this._abandonButton_Click);
            // 
            // _historyBox
            // 
            this._historyBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._historyBox.FormattingEnabled = true;
            this._historyBox.IntegralHeight = false;
            this._historyBox.ItemHeight = 16;
            this._historyBox.Location = new System.Drawing.Point(4, 131);
            this._historyBox.Margin = new System.Windows.Forms.Padding(4);
            this._historyBox.Name = "_historyBox";
            this._historyBox.Size = new System.Drawing.Size(340, 265);
            this._historyBox.TabIndex = 5;
            // 
            // _showCoverage
            // 
            this._showCoverage.AutoSize = true;
            this._showCoverage.Location = new System.Drawing.Point(5, 67);
            this._showCoverage.Name = "_showCoverage";
            this._showCoverage.Size = new System.Drawing.Size(129, 21);
            this._showCoverage.TabIndex = 6;
            this._showCoverage.Text = "Show Coverage";
            this._showCoverage.UseVisualStyleBackColor = true;
            this._showCoverage.CheckedChanged += new System.EventHandler(this._visualizeCoverage_CheckedChanged);
            // 
            // TerrainContextControls
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._showCoverage);
            this.Controls.Add(this._historyBox);
            this.Controls.Add(this._abandonButton);
            this.Controls.Add(this._saveButton);
            this.Controls.Add(this._lockedAreaLabel);
            this.Controls.Add(this._showLockedArea);
            this.Controls.Add(this._coverageLayer);
            this.Margin = new System.Windows.Forms.Padding(4);
            this.Name = "TerrainContextControls";
            this.Size = new System.Drawing.Size(350, 400);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox _coverageLayer;
        private System.Windows.Forms.CheckBox _showLockedArea;
        private System.Windows.Forms.Label _lockedAreaLabel;
        private System.Windows.Forms.Button _saveButton;
        private System.Windows.Forms.Button _abandonButton;
        private System.Windows.Forms.ListBox _historyBox;
        private System.Windows.Forms.CheckBox _showCoverage;
    }
}
