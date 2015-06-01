// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace LevelEditorXLE.Terrain
{
    partial class TerrainConfig
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
            this.m_propertyGrid1 = new System.Windows.Forms.PropertyGrid();
            this.m_okButton = new System.Windows.Forms.Button();
            this.m_cancelButton = new System.Windows.Forms.Button();
            this.m_doImport = new System.Windows.Forms.CheckBox();
            this.m_importType = new System.Windows.Forms.ComboBox();
            this.m_label1 = new System.Windows.Forms.Label();
            this.m_importSource = new System.Windows.Forms.TextBox();
            this.m_importSourceBtn = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // m_propertyGrid1
            // 
            this.m_propertyGrid1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.m_propertyGrid1.CategoryForeColor = System.Drawing.SystemColors.InactiveCaptionText;
            this.m_propertyGrid1.Location = new System.Drawing.Point(12, 12);
            this.m_propertyGrid1.Name = "m_propertyGrid1";
            this.m_propertyGrid1.Size = new System.Drawing.Size(556, 224);
            this.m_propertyGrid1.TabIndex = 0;
            this.m_propertyGrid1.ToolbarVisible = false;
            // 
            // m_okButton
            // 
            this.m_okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.m_okButton.Location = new System.Drawing.Point(448, 274);
            this.m_okButton.Name = "m_okButton";
            this.m_okButton.Size = new System.Drawing.Size(119, 23);
            this.m_okButton.TabIndex = 1;
            this.m_okButton.Text = "OK";
            this.m_okButton.UseVisualStyleBackColor = true;
            // 
            // m_cancelButton
            // 
            this.m_cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.m_cancelButton.Location = new System.Drawing.Point(323, 274);
            this.m_cancelButton.Name = "m_cancelButton";
            this.m_cancelButton.Size = new System.Drawing.Size(119, 23);
            this.m_cancelButton.TabIndex = 2;
            this.m_cancelButton.Text = "Cancel";
            this.m_cancelButton.UseVisualStyleBackColor = true;
            // 
            // m_doImport
            // 
            this.m_doImport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_doImport.AutoSize = true;
            this.m_doImport.Location = new System.Drawing.Point(13, 244);
            this.m_doImport.Name = "m_doImport";
            this.m_doImport.Size = new System.Drawing.Size(110, 17);
            this.m_doImport.TabIndex = 3;
            this.m_doImport.Text = "Import new terrain";
            this.m_doImport.UseVisualStyleBackColor = true;
            this.m_doImport.CheckedChanged += new System.EventHandler(this.DoImport_CheckedChanged);
            // 
            // m_importType
            // 
            this.m_importType.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_importType.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.m_importType.FormattingEnabled = true;
            this.m_importType.Items.AddRange(new object[] {
            "DEM file"});
            this.m_importType.Location = new System.Drawing.Point(129, 242);
            this.m_importType.Name = "m_importType";
            this.m_importType.Size = new System.Drawing.Size(83, 21);
            this.m_importType.TabIndex = 4;
            this.m_importType.SelectedIndexChanged += new System.EventHandler(this.ImportType_SelectedIndexChanged);
            // 
            // m_label1
            // 
            this.m_label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_label1.AutoSize = true;
            this.m_label1.Location = new System.Drawing.Point(218, 245);
            this.m_label1.Name = "m_label1";
            this.m_label1.Size = new System.Drawing.Size(44, 13);
            this.m_label1.TabIndex = 5;
            this.m_label1.Text = "Source:";
            // 
            // m_importSource
            // 
            this.m_importSource.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importSource.Location = new System.Drawing.Point(268, 242);
            this.m_importSource.Name = "m_importSource";
            this.m_importSource.Size = new System.Drawing.Size(270, 20);
            this.m_importSource.TabIndex = 6;
            // 
            // m_importSourceBtn
            // 
            this.m_importSourceBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importSourceBtn.Location = new System.Drawing.Point(544, 243);
            this.m_importSourceBtn.Name = "m_importSourceBtn";
            this.m_importSourceBtn.Size = new System.Drawing.Size(23, 18);
            this.m_importSourceBtn.TabIndex = 7;
            this.m_importSourceBtn.Text = "...";
            this.m_importSourceBtn.UseVisualStyleBackColor = true;
            this.m_importSourceBtn.Click += new System.EventHandler(this.ImportSourceBtn_Click);
            // 
            // TerrainConfig
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(580, 309);
            this.Controls.Add(this.m_importSourceBtn);
            this.Controls.Add(this.m_importSource);
            this.Controls.Add(this.m_label1);
            this.Controls.Add(this.m_importType);
            this.Controls.Add(this.m_doImport);
            this.Controls.Add(this.m_cancelButton);
            this.Controls.Add(this.m_okButton);
            this.Controls.Add(this.m_propertyGrid1);
            this.Name = "TerrainConfig";
            this.Text = "Configure Terrain";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        // private Sce.Atf.Controls.PropertyEditing.PropertyGrid m_propertyGrid1;
        private System.Windows.Forms.PropertyGrid m_propertyGrid1;
        private System.Windows.Forms.Button m_okButton;
        private System.Windows.Forms.Button m_cancelButton;
        private System.Windows.Forms.CheckBox m_doImport;
        private System.Windows.Forms.ComboBox m_importType;
        private System.Windows.Forms.Label m_label1;
        private System.Windows.Forms.TextBox m_importSource;
        private System.Windows.Forms.Button m_importSourceBtn;
    }
}