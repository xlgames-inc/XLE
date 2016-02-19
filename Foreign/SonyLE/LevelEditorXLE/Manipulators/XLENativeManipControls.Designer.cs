// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace LevelEditorXLE.Manipulators
{
    partial class XLENativeManipControls
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
            this.manipulatorSelection = new System.Windows.Forms.ListBox();
            this.manipulatorProperties = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
            this.SuspendLayout();
            // 
            // manipulatorSelection
            // 
            this.manipulatorSelection.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.manipulatorSelection.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.manipulatorSelection.FormattingEnabled = true;
            this.manipulatorSelection.IntegralHeight = false;
            this.manipulatorSelection.ItemHeight = 16;
            this.manipulatorSelection.Location = new System.Drawing.Point(0, 0);
            this.manipulatorSelection.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.manipulatorSelection.Name = "manipulatorSelection";
            this.manipulatorSelection.Size = new System.Drawing.Size(203, 246);
            this.manipulatorSelection.TabIndex = 0;
            this.manipulatorSelection.SelectedIndexChanged += new System.EventHandler(this.manipulatorSelection_SelectedIndexChanged);
            // 
            // manipulatorProperties
            // 
            this.manipulatorProperties.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.manipulatorProperties.Font = new System.Drawing.Font("Segoe UI", 9F);
            this.manipulatorProperties.Location = new System.Drawing.Point(211, 0);
            this.manipulatorProperties.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.manipulatorProperties.Name = "manipulatorProperties";
            this.manipulatorProperties.PropertySorting = ((Sce.Atf.Controls.PropertyEditing.PropertySorting)(((Sce.Atf.Controls.PropertyEditing.PropertySorting.Categorized | Sce.Atf.Controls.PropertyEditing.PropertySorting.Alphabetical) 
            | Sce.Atf.Controls.PropertyEditing.PropertySorting.CategoryAlphabetical)));
            this.manipulatorProperties.SelectedPropertyDescriptor = null;
            this.manipulatorProperties.Settings = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?><PropertyView PropertySort" +
    "ing=\"ByCategory\" />";
            this.manipulatorProperties.Size = new System.Drawing.Size(458, 246);
            this.manipulatorProperties.TabIndex = 1;
            // 
            // XLENativeManipControls
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.manipulatorProperties);
            this.Controls.Add(this.manipulatorSelection);
            this.Margin = new System.Windows.Forms.Padding(0);
            this.Name = "XLENativeManipControls";
            this.Size = new System.Drawing.Size(669, 246);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ListBox manipulatorSelection;
        private Sce.Atf.Controls.PropertyEditing.PropertyGrid manipulatorProperties;
    }
}
