// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace LevelEditorXLE.Terrain
{
    partial class TerrainCoverageConfig
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
            this.m_createBlankTerrainCheck = new System.Windows.Forms.CheckBox();
            this.m_importWarnings = new System.Windows.Forms.Button();
            this.m_importFormat = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // m_propertyGrid1
            // 
            this.m_propertyGrid1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.m_propertyGrid1.CategoryForeColor = System.Drawing.SystemColors.InactiveCaptionText;
            this.m_propertyGrid1.Location = new System.Drawing.Point(16, 15);
            this.m_propertyGrid1.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_propertyGrid1.Name = "m_propertyGrid1";
            this.m_propertyGrid1.Size = new System.Drawing.Size(652, 246);
            this.m_propertyGrid1.TabIndex = 0;
            this.m_propertyGrid1.ToolbarVisible = false;
            // 
            // m_okButton
            // 
            this.m_okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.m_okButton.Location = new System.Drawing.Point(508, 382);
            this.m_okButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_okButton.Name = "m_okButton";
            this.m_okButton.Size = new System.Drawing.Size(159, 28);
            this.m_okButton.TabIndex = 1;
            this.m_okButton.Text = "OK";
            this.m_okButton.UseVisualStyleBackColor = true;
            // 
            // m_cancelButton
            // 
            this.m_cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.m_cancelButton.Location = new System.Drawing.Point(341, 382);
            this.m_cancelButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_cancelButton.Name = "m_cancelButton";
            this.m_cancelButton.Size = new System.Drawing.Size(159, 28);
            this.m_cancelButton.TabIndex = 2;
            this.m_cancelButton.Text = "Cancel";
            this.m_cancelButton.UseVisualStyleBackColor = true;
            // 
            // m_doImport
            // 
            this.m_doImport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_doImport.AutoSize = true;
            this.m_doImport.Location = new System.Drawing.Point(17, 271);
            this.m_doImport.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_doImport.Name = "m_doImport";
            this.m_doImport.Size = new System.Drawing.Size(69, 21);
            this.m_doImport.TabIndex = 3;
            this.m_doImport.Text = "Import";
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
            this.m_importType.Location = new System.Drawing.Point(172, 268);
            this.m_importType.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_importType.Name = "m_importType";
            this.m_importType.Size = new System.Drawing.Size(109, 24);
            this.m_importType.TabIndex = 4;
            this.m_importType.SelectedIndexChanged += new System.EventHandler(this.ImportType_SelectedIndexChanged);
            // 
            // m_label1
            // 
            this.m_label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_label1.AutoSize = true;
            this.m_label1.Location = new System.Drawing.Point(291, 272);
            this.m_label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.m_label1.Name = "m_label1";
            this.m_label1.Size = new System.Drawing.Size(57, 17);
            this.m_label1.TabIndex = 5;
            this.m_label1.Text = "Source:";
            // 
            // m_importSource
            // 
            this.m_importSource.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importSource.Location = new System.Drawing.Point(357, 268);
            this.m_importSource.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_importSource.Name = "m_importSource";
            this.m_importSource.Size = new System.Drawing.Size(269, 22);
            this.m_importSource.TabIndex = 6;
            this.m_importSource.TextChanged += new System.EventHandler(this.m_importSource_TextChanged);
            // 
            // m_importSourceBtn
            // 
            this.m_importSourceBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importSourceBtn.Location = new System.Drawing.Point(636, 270);
            this.m_importSourceBtn.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_importSourceBtn.Name = "m_importSourceBtn";
            this.m_importSourceBtn.Size = new System.Drawing.Size(31, 22);
            this.m_importSourceBtn.TabIndex = 7;
            this.m_importSourceBtn.Text = "...";
            this.m_importSourceBtn.UseVisualStyleBackColor = true;
            this.m_importSourceBtn.Click += new System.EventHandler(this.ImportSourceBtn_Click);
            // 
            // m_createBlankTerrainCheck
            // 
            this.m_createBlankTerrainCheck.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.m_createBlankTerrainCheck.AutoSize = true;
            this.m_createBlankTerrainCheck.Location = new System.Drawing.Point(16, 338);
            this.m_createBlankTerrainCheck.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_createBlankTerrainCheck.Name = "m_createBlankTerrainCheck";
            this.m_createBlankTerrainCheck.Size = new System.Drawing.Size(111, 21);
            this.m_createBlankTerrainCheck.TabIndex = 3;
            this.m_createBlankTerrainCheck.Text = "Create Blank";
            this.m_createBlankTerrainCheck.UseVisualStyleBackColor = true;
            this.m_createBlankTerrainCheck.CheckedChanged += new System.EventHandler(this.DoCreateBlank_CheckedChanged);
            // 
            // m_importWarnings
            // 
            this.m_importWarnings.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importWarnings.Location = new System.Drawing.Point(563, 298);
            this.m_importWarnings.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_importWarnings.Name = "m_importWarnings";
            this.m_importWarnings.Size = new System.Drawing.Size(100, 28);
            this.m_importWarnings.TabIndex = 15;
            this.m_importWarnings.Text = "Warnings";
            this.m_importWarnings.UseVisualStyleBackColor = true;
            this.m_importWarnings.Click += new System.EventHandler(this.m_importWarnings_Click);
            // 
            // m_importFormat
            // 
            this.m_importFormat.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.m_importFormat.Location = new System.Drawing.Point(421, 302);
            this.m_importFormat.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.m_importFormat.Name = "m_importFormat";
            this.m_importFormat.Size = new System.Drawing.Size(132, 22);
            this.m_importFormat.TabIndex = 16;
            this.m_importFormat.TextChanged += new System.EventHandler(this.m_importFormat_TextChanged);
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(315, 305);
            this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(99, 17);
            this.label1.TabIndex = 17;
            this.label1.Text = "Import Format:";
            // 
            // TerrainCoverageConfig
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(684, 426);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.m_importFormat);
            this.Controls.Add(this.m_importWarnings);
            this.Controls.Add(this.m_importSourceBtn);
            this.Controls.Add(this.m_importSource);
            this.Controls.Add(this.m_label1);
            this.Controls.Add(this.m_importType);
            this.Controls.Add(this.m_createBlankTerrainCheck);
            this.Controls.Add(this.m_doImport);
            this.Controls.Add(this.m_cancelButton);
            this.Controls.Add(this.m_okButton);
            this.Controls.Add(this.m_propertyGrid1);
            this.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.Name = "TerrainCoverageConfig";
            this.Text = "Configure Coverage Layer";
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
        private System.Windows.Forms.CheckBox m_createBlankTerrainCheck;
        private System.Windows.Forms.Button m_importWarnings;
        private System.Windows.Forms.TextBox m_importFormat;
        private System.Windows.Forms.Label label1;
    }
}