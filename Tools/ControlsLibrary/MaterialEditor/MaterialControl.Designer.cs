// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibrary.MaterialEditor
{
    partial class MaterialControl
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
                ClearAndDispose();
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
            this.materialPreview1 = new MaterialPreview();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.shaderConstants = new System.Windows.Forms.DataGridView();
            this.splitContainer3 = new System.Windows.Forms.SplitContainer();
            this.resourceBindings = new System.Windows.Forms.DataGridView();
            this.materialParameterBox = new System.Windows.Forms.DataGridView();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.checkBox2 = new System.Windows.Forms.CheckBox();
            this.checkBox1 = new System.Windows.Forms.CheckBox();
            this._pages = new System.Windows.Forms.TabControl();
            this._technicalPage = new System.Windows.Forms.TabPage();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.shaderConstants)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer3)).BeginInit();
            this.splitContainer3.Panel1.SuspendLayout();
            this.splitContainer3.Panel2.SuspendLayout();
            this.splitContainer3.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.resourceBindings)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.materialParameterBox)).BeginInit();
            this.groupBox1.SuspendLayout();
            this._pages.SuspendLayout();
            this._technicalPage.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.Location = new System.Drawing.Point(0, 0);
            this.splitContainer1.Name = "splitContainer1";
            this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.materialPreview1);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this._pages);
            this.splitContainer1.Panel2.Controls.Add(this.groupBox1);
            this.splitContainer1.Size = new System.Drawing.Size(275, 515);
            this.splitContainer1.SplitterDistance = 209;
            this.splitContainer1.TabIndex = 0;
            // 
            // materialPreview1
            // 
            this.materialPreview1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.materialPreview1.Location = new System.Drawing.Point(0, 0);
            this.materialPreview1.Name = "materialPreview1";
            this.materialPreview1.Size = new System.Drawing.Size(275, 209);
            this.materialPreview1.TabIndex = 0;
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(3, 3);
            this.splitContainer2.Name = "splitContainer2";
            this.splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.shaderConstants);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.splitContainer3);
            this.splitContainer2.Size = new System.Drawing.Size(261, 214);
            this.splitContainer2.SplitterDistance = 67;
            this.splitContainer2.TabIndex = 4;
            // 
            // shaderConstants
            // 
            this.shaderConstants.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.shaderConstants.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this.shaderConstants.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.shaderConstants.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.shaderConstants.ColumnHeadersVisible = false;
            this.shaderConstants.Dock = System.Windows.Forms.DockStyle.Fill;
            this.shaderConstants.Location = new System.Drawing.Point(0, 0);
            this.shaderConstants.Name = "shaderConstants";
            this.shaderConstants.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.shaderConstants.Size = new System.Drawing.Size(261, 67);
            this.shaderConstants.TabIndex = 2;
            // 
            // splitContainer3
            // 
            this.splitContainer3.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer3.Location = new System.Drawing.Point(0, 0);
            this.splitContainer3.Name = "splitContainer3";
            this.splitContainer3.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer3.Panel1
            // 
            this.splitContainer3.Panel1.Controls.Add(this.resourceBindings);
            // 
            // splitContainer3.Panel2
            // 
            this.splitContainer3.Panel2.Controls.Add(this.materialParameterBox);
            this.splitContainer3.Size = new System.Drawing.Size(261, 143);
            this.splitContainer3.SplitterDistance = 71;
            this.splitContainer3.TabIndex = 0;
            // 
            // resourceBindings
            // 
            this.resourceBindings.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.resourceBindings.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this.resourceBindings.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.resourceBindings.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.resourceBindings.ColumnHeadersVisible = false;
            this.resourceBindings.Dock = System.Windows.Forms.DockStyle.Fill;
            this.resourceBindings.Location = new System.Drawing.Point(0, 0);
            this.resourceBindings.Name = "resourceBindings";
            this.resourceBindings.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.resourceBindings.RowHeadersWidthSizeMode = System.Windows.Forms.DataGridViewRowHeadersWidthSizeMode.DisableResizing;
            this.resourceBindings.Size = new System.Drawing.Size(261, 71);
            this.resourceBindings.TabIndex = 0;
            // 
            // materialParameterBox
            // 
            this.materialParameterBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.materialParameterBox.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this.materialParameterBox.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.materialParameterBox.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.materialParameterBox.ColumnHeadersVisible = false;
            this.materialParameterBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this.materialParameterBox.Location = new System.Drawing.Point(0, 0);
            this.materialParameterBox.Name = "materialParameterBox";
            this.materialParameterBox.Size = new System.Drawing.Size(261, 68);
            this.materialParameterBox.TabIndex = 1;
            // 
            // groupBox1
            // 
            this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.groupBox1.Controls.Add(this.checkBox2);
            this.groupBox1.Controls.Add(this.checkBox1);
            this.groupBox1.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.groupBox1.Location = new System.Drawing.Point(3, 252);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(269, 47);
            this.groupBox1.TabIndex = 3;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "States";
            // 
            // checkBox2
            // 
            this.checkBox2.AutoSize = true;
            this.checkBox2.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.checkBox2.Location = new System.Drawing.Point(98, 20);
            this.checkBox2.Name = "checkBox2";
            this.checkBox2.Size = new System.Drawing.Size(71, 17);
            this.checkBox2.TabIndex = 1;
            this.checkBox2.Text = "Wireframe";
            this.checkBox2.ThreeState = true;
            this.checkBox2.UseVisualStyleBackColor = true;
            // 
            // checkBox1
            // 
            this.checkBox1.AutoSize = true;
            this.checkBox1.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this.checkBox1.Location = new System.Drawing.Point(7, 20);
            this.checkBox1.Name = "checkBox1";
            this.checkBox1.Size = new System.Drawing.Size(85, 17);
            this.checkBox1.TabIndex = 0;
            this.checkBox1.Text = "Double sided";
            this.checkBox1.ThreeState = true;
            this.checkBox1.UseVisualStyleBackColor = true;
            // 
            // _pages
            // 
            this._pages.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._pages.Controls.Add(this._technicalPage);
            this._pages.Location = new System.Drawing.Point(0, 0);
            this._pages.Name = "_pages";
            this._pages.SelectedIndex = 0;
            this._pages.Size = new System.Drawing.Size(275, 246);
            this._pages.TabIndex = 3;
            // 
            // _technicalPage
            // 
            this._technicalPage.Controls.Add(this.splitContainer2);
            this._technicalPage.Location = new System.Drawing.Point(4, 22);
            this._technicalPage.Name = "_technicalPage";
            this._technicalPage.Padding = new System.Windows.Forms.Padding(3);
            this._technicalPage.Size = new System.Drawing.Size(267, 220);
            this._technicalPage.TabIndex = 0;
            this._technicalPage.Text = "Technical";
            this._technicalPage.UseVisualStyleBackColor = true;
            // 
            // MaterialControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.Name = "MaterialControl";
            this.Size = new System.Drawing.Size(275, 515);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.shaderConstants)).EndInit();
            this.splitContainer3.Panel1.ResumeLayout(false);
            this.splitContainer3.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer3)).EndInit();
            this.splitContainer3.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.resourceBindings)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.materialParameterBox)).EndInit();
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this._pages.ResumeLayout(false);
            this._technicalPage.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.CheckBox checkBox2;
        private System.Windows.Forms.CheckBox checkBox1;
        private System.Windows.Forms.DataGridView shaderConstants;
        private System.Windows.Forms.DataGridView materialParameterBox;
        private System.Windows.Forms.DataGridView resourceBindings;
        private MaterialPreview materialPreview1;
        // private System.Windows.Forms.Button materialPreview1;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.SplitContainer splitContainer3;
        private System.Windows.Forms.TabControl _pages;
        private System.Windows.Forms.TabPage _technicalPage;
    }
}
