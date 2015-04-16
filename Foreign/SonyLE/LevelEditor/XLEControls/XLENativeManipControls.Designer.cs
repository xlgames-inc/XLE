namespace LevelEditor.XLEControls
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
            this.manipulatorProperties = new Sce.Atf.Controls.PropertyEditing.PropertyGrid(
                Sce.Atf.Controls.PropertyEditing.PropertyGridMode.DisableSearchControls
                | Sce.Atf.Controls.PropertyEditing.PropertyGridMode.HideResetAllButton);
            this.SuspendLayout();
            // 
            // manipulatorSelection
            // 
            this.manipulatorSelection.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.manipulatorSelection.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.manipulatorSelection.FormattingEnabled = true;
            this.manipulatorSelection.Location = new System.Drawing.Point(3, 3);
            this.manipulatorSelection.Name = "manipulatorSelection";
            this.manipulatorSelection.Size = new System.Drawing.Size(149, 182);
            this.manipulatorSelection.TabIndex = 0;
            this.manipulatorSelection.SelectedIndexChanged += new System.EventHandler(this.manipulatorSelection_SelectedIndexChanged);
            // 
            // manipulatorProperties
            // 
            this.manipulatorProperties.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            // this.manipulatorProperties.HelpVisible = false;
            this.manipulatorProperties.Location = new System.Drawing.Point(158, 3);
            this.manipulatorProperties.Name = "manipulatorProperties";
            this.manipulatorProperties.Size = new System.Drawing.Size(341, 188);
            this.manipulatorProperties.TabIndex = 1;
            // this.manipulatorProperties.ToolbarVisible = false;
            // 
            // TerrainManipControls
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.manipulatorProperties);
            this.Controls.Add(this.manipulatorSelection);
            this.Name = "TerrainManipControls";
            this.Size = new System.Drawing.Size(502, 200);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.ListBox manipulatorSelection;
        private Sce.Atf.Controls.PropertyEditing.PropertyGrid manipulatorProperties;
    }
}
