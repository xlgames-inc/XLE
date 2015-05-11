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
            if (disposing)
            {
                ClearAndDispose();
                _materialPreview1.Dispose();
                if (components != null) components.Dispose();
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
            this._splitContainer1 = new System.Windows.Forms.SplitContainer();
            this._materialPreview1 = new MaterialPreview();
            this._pages = new System.Windows.Forms.TabControl();
            this._technicalPage = new System.Windows.Forms.TabPage();
            this._splitContainer2 = new System.Windows.Forms.SplitContainer();
            this._shaderConstants = new System.Windows.Forms.DataGridView();
            this._splitContainer3 = new System.Windows.Forms.SplitContainer();
            this._resourceBindings = new System.Windows.Forms.DataGridView();
            this._materialParameterBox = new System.Windows.Forms.DataGridView();
            this._statesGroup = new System.Windows.Forms.GroupBox();
            this._blendMode = new System.Windows.Forms.ComboBox();
            this._wireframeGroup = new System.Windows.Forms.CheckBox();
            this._doubleSidedCheck = new System.Windows.Forms.CheckBox();
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer1)).BeginInit();
            this._splitContainer1.Panel1.SuspendLayout();
            this._splitContainer1.Panel2.SuspendLayout();
            this._splitContainer1.SuspendLayout();
            this._pages.SuspendLayout();
            this._technicalPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer2)).BeginInit();
            this._splitContainer2.Panel1.SuspendLayout();
            this._splitContainer2.Panel2.SuspendLayout();
            this._splitContainer2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._shaderConstants)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer3)).BeginInit();
            this._splitContainer3.Panel1.SuspendLayout();
            this._splitContainer3.Panel2.SuspendLayout();
            this._splitContainer3.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._resourceBindings)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._materialParameterBox)).BeginInit();
            this._statesGroup.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this._splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitContainer1.Location = new System.Drawing.Point(0, 0);
            this._splitContainer1.Name = "splitContainer1";
            this._splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer1.Panel1
            // 
            this._splitContainer1.Panel1.Controls.Add(this._materialPreview1);
            // 
            // splitContainer1.Panel2
            // 
            this._splitContainer1.Panel2.Controls.Add(this._pages);
            this._splitContainer1.Panel2.Controls.Add(this._statesGroup);
            this._splitContainer1.Size = new System.Drawing.Size(272, 512);
            this._splitContainer1.SplitterDistance = 207;
            this._splitContainer1.TabIndex = 0;
            // 
            // materialPreview1
            // 
            this._materialPreview1.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialPreview1.Location = new System.Drawing.Point(0, 0);
            this._materialPreview1.Name = "materialPreview1";
            this._materialPreview1.Size = new System.Drawing.Size(272, 207);
            this._materialPreview1.TabIndex = 0;
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
            this._pages.Size = new System.Drawing.Size(272, 245);
            this._pages.TabIndex = 3;
            // 
            // _technicalPage
            // 
            this._technicalPage.Controls.Add(this._splitContainer2);
            this._technicalPage.Location = new System.Drawing.Point(4, 22);
            this._technicalPage.Name = "_technicalPage";
            this._technicalPage.Padding = new System.Windows.Forms.Padding(3);
            this._technicalPage.Size = new System.Drawing.Size(264, 219);
            this._technicalPage.TabIndex = 0;
            this._technicalPage.Text = "Technical";
            this._technicalPage.UseVisualStyleBackColor = true;
            // 
            // splitContainer2
            // 
            this._splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitContainer2.Location = new System.Drawing.Point(3, 3);
            this._splitContainer2.Name = "splitContainer2";
            this._splitContainer2.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer2.Panel1
            // 
            this._splitContainer2.Panel1.Controls.Add(this._shaderConstants);
            // 
            // splitContainer2.Panel2
            // 
            this._splitContainer2.Panel2.Controls.Add(this._splitContainer3);
            this._splitContainer2.Size = new System.Drawing.Size(258, 213);
            this._splitContainer2.SplitterDistance = 66;
            this._splitContainer2.TabIndex = 4;
            // 
            // shaderConstants
            // 
            this._shaderConstants.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._shaderConstants.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._shaderConstants.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._shaderConstants.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._shaderConstants.ColumnHeadersVisible = false;
            this._shaderConstants.Dock = System.Windows.Forms.DockStyle.Fill;
            this._shaderConstants.Location = new System.Drawing.Point(0, 0);
            this._shaderConstants.Name = "shaderConstants";
            this._shaderConstants.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._shaderConstants.Size = new System.Drawing.Size(258, 66);
            this._shaderConstants.TabIndex = 2;
            // 
            // splitContainer3
            // 
            this._splitContainer3.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitContainer3.Location = new System.Drawing.Point(0, 0);
            this._splitContainer3.Name = "splitContainer3";
            this._splitContainer3.Orientation = System.Windows.Forms.Orientation.Horizontal;
            // 
            // splitContainer3.Panel1
            // 
            this._splitContainer3.Panel1.Controls.Add(this._resourceBindings);
            // 
            // splitContainer3.Panel2
            // 
            this._splitContainer3.Panel2.Controls.Add(this._materialParameterBox);
            this._splitContainer3.Size = new System.Drawing.Size(258, 143);
            this._splitContainer3.SplitterDistance = 71;
            this._splitContainer3.TabIndex = 0;
            // 
            // resourceBindings
            // 
            this._resourceBindings.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._resourceBindings.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._resourceBindings.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._resourceBindings.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._resourceBindings.ColumnHeadersVisible = false;
            this._resourceBindings.Dock = System.Windows.Forms.DockStyle.Fill;
            this._resourceBindings.Location = new System.Drawing.Point(0, 0);
            this._resourceBindings.Name = "resourceBindings";
            this._resourceBindings.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._resourceBindings.RowHeadersWidthSizeMode = System.Windows.Forms.DataGridViewRowHeadersWidthSizeMode.DisableResizing;
            this._resourceBindings.Size = new System.Drawing.Size(258, 71);
            this._resourceBindings.TabIndex = 0;
            // 
            // materialParameterBox
            // 
            this._materialParameterBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._materialParameterBox.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._materialParameterBox.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._materialParameterBox.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._materialParameterBox.ColumnHeadersVisible = false;
            this._materialParameterBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialParameterBox.Location = new System.Drawing.Point(0, 0);
            this._materialParameterBox.Name = "materialParameterBox";
            this._materialParameterBox.Size = new System.Drawing.Size(258, 68);
            this._materialParameterBox.TabIndex = 1;
            // 
            // groupBox1
            // 
            this._statesGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._statesGroup.Controls.Add(this._blendMode);
            this._statesGroup.Controls.Add(this._wireframeGroup);
            this._statesGroup.Controls.Add(this._doubleSidedCheck);
            this._statesGroup.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._statesGroup.Location = new System.Drawing.Point(3, 251);
            this._statesGroup.Name = "groupBox1";
            this._statesGroup.Size = new System.Drawing.Size(266, 47);
            this._statesGroup.TabIndex = 3;
            this._statesGroup.TabStop = false;
            this._statesGroup.Text = "States";
            // 
            // _blendMode
            // 
            this._blendMode.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._blendMode.FormattingEnabled = true;
            this._blendMode.Location = new System.Drawing.Point(175, 16);
            this._blendMode.Name = "_blendMode";
            this._blendMode.Size = new System.Drawing.Size(84, 21);
            this._blendMode.TabIndex = 2;
            this._blendMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            // 
            // checkBox2
            // 
            this._wireframeGroup.AutoSize = true;
            this._wireframeGroup.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._wireframeGroup.Location = new System.Drawing.Point(98, 20);
            this._wireframeGroup.Name = "checkBox2";
            this._wireframeGroup.Size = new System.Drawing.Size(71, 17);
            this._wireframeGroup.TabIndex = 1;
            this._wireframeGroup.Text = "Wireframe";
            this._wireframeGroup.ThreeState = true;
            this._wireframeGroup.UseVisualStyleBackColor = true;
            // 
            // checkBox1
            // 
            this._doubleSidedCheck.AutoSize = true;
            this._doubleSidedCheck.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._doubleSidedCheck.Location = new System.Drawing.Point(7, 20);
            this._doubleSidedCheck.Name = "checkBox1";
            this._doubleSidedCheck.Size = new System.Drawing.Size(85, 17);
            this._doubleSidedCheck.TabIndex = 0;
            this._doubleSidedCheck.Text = "Double sided";
            this._doubleSidedCheck.ThreeState = true;
            this._doubleSidedCheck.UseVisualStyleBackColor = true;
            // 
            // MaterialControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._splitContainer1);
            this.Name = "MaterialControl";
            this.Size = new System.Drawing.Size(272, 512);
            this._splitContainer1.Panel1.ResumeLayout(false);
            this._splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer1)).EndInit();
            this._splitContainer1.ResumeLayout(false);
            this._pages.ResumeLayout(false);
            this._technicalPage.ResumeLayout(false);
            this._splitContainer2.Panel1.ResumeLayout(false);
            this._splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer2)).EndInit();
            this._splitContainer2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._shaderConstants)).EndInit();
            this._splitContainer3.Panel1.ResumeLayout(false);
            this._splitContainer3.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer3)).EndInit();
            this._splitContainer3.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._resourceBindings)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._materialParameterBox)).EndInit();
            this._statesGroup.ResumeLayout(false);
            this._statesGroup.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer _splitContainer1;
        private System.Windows.Forms.GroupBox _statesGroup;
        private System.Windows.Forms.CheckBox _wireframeGroup;
        private System.Windows.Forms.CheckBox _doubleSidedCheck;
        private System.Windows.Forms.DataGridView _shaderConstants;
        private System.Windows.Forms.DataGridView _materialParameterBox;
        private System.Windows.Forms.DataGridView _resourceBindings;
        private MaterialPreview _materialPreview1;
        // private System.Windows.Forms.Button materialPreview1;
        private System.Windows.Forms.SplitContainer _splitContainer2;
        private System.Windows.Forms.SplitContainer _splitContainer3;
        private System.Windows.Forms.TabControl _pages;
        private System.Windows.Forms.TabPage _technicalPage;
        private System.Windows.Forms.ComboBox _blendMode;
    }
}
