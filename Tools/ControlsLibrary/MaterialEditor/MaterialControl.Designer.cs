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
                ClearAndDispose(true);
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
            this._pages = new System.Windows.Forms.TabControl();
            this._constantsPage = new System.Windows.Forms.TabPage();
            this._shaderConstants = new System.Windows.Forms.DataGridView();
            this._resourceBindings = new System.Windows.Forms.DataGridView();
            this._materialParameterBox = new System.Windows.Forms.DataGridView();
            this._statesGroup = new System.Windows.Forms.GroupBox();
            this._blendMode = new System.Windows.Forms.ComboBox();
            this._wireframeGroup = new System.Windows.Forms.CheckBox();
            this._doubleSidedCheck = new System.Windows.Forms.CheckBox();
            this._resourcesPage = new System.Windows.Forms.TabPage();
            this._shaderFlags = new System.Windows.Forms.TabPage();
            this._pages.SuspendLayout();
            this._constantsPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._shaderConstants)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._resourceBindings)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._materialParameterBox)).BeginInit();
            this._statesGroup.SuspendLayout();
            this._resourcesPage.SuspendLayout();
            this._shaderFlags.SuspendLayout();
            this.SuspendLayout();
            // 
            // _pages
            // 
            this._pages.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._pages.Controls.Add(this._resourcesPage);
            this._pages.Controls.Add(this._constantsPage);
            this._pages.Controls.Add(this._shaderFlags);
            this._pages.Location = new System.Drawing.Point(0, 0);
            this._pages.Name = "_pages";
            this._pages.SelectedIndex = 0;
            this._pages.Size = new System.Drawing.Size(525, 275);
            this._pages.TabIndex = 3;
            // 
            // _constantsPage
            // 
            this._constantsPage.Controls.Add(this._shaderConstants);
            this._constantsPage.Location = new System.Drawing.Point(4, 22);
            this._constantsPage.Name = "_constantsPage";
            this._constantsPage.Padding = new System.Windows.Forms.Padding(3);
            this._constantsPage.Size = new System.Drawing.Size(517, 249);
            this._constantsPage.TabIndex = 0;
            this._constantsPage.Text = "Constants";
            this._constantsPage.UseVisualStyleBackColor = true;
            // 
            // _shaderConstants
            // 
            this._shaderConstants.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._shaderConstants.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._shaderConstants.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._shaderConstants.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._shaderConstants.ColumnHeadersVisible = false;
            this._shaderConstants.Dock = System.Windows.Forms.DockStyle.Fill;
            this._shaderConstants.Location = new System.Drawing.Point(3, 3);
            this._shaderConstants.Name = "_shaderConstants";
            this._shaderConstants.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._shaderConstants.Size = new System.Drawing.Size(511, 243);
            this._shaderConstants.TabIndex = 2;
            // 
            // _resourceBindings
            // 
            this._resourceBindings.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._resourceBindings.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._resourceBindings.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._resourceBindings.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._resourceBindings.ColumnHeadersVisible = false;
            this._resourceBindings.Dock = System.Windows.Forms.DockStyle.Fill;
            this._resourceBindings.Location = new System.Drawing.Point(0, 0);
            this._resourceBindings.Name = "_resourceBindings";
            this._resourceBindings.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._resourceBindings.RowHeadersWidthSizeMode = System.Windows.Forms.DataGridViewRowHeadersWidthSizeMode.DisableResizing;
            this._resourceBindings.Size = new System.Drawing.Size(517, 249);
            this._resourceBindings.TabIndex = 0;
            // 
            // _materialParameterBox
            // 
            this._materialParameterBox.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._materialParameterBox.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            this._materialParameterBox.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this._materialParameterBox.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._materialParameterBox.ColumnHeadersVisible = false;
            this._materialParameterBox.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialParameterBox.Location = new System.Drawing.Point(0, 0);
            this._materialParameterBox.Name = "_materialParameterBox";
            this._materialParameterBox.Size = new System.Drawing.Size(517, 249);
            this._materialParameterBox.TabIndex = 1;
            // 
            // _statesGroup
            // 
            this._statesGroup.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._statesGroup.Controls.Add(this._blendMode);
            this._statesGroup.Controls.Add(this._wireframeGroup);
            this._statesGroup.Controls.Add(this._doubleSidedCheck);
            this._statesGroup.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._statesGroup.Location = new System.Drawing.Point(3, 281);
            this._statesGroup.Name = "_statesGroup";
            this._statesGroup.Size = new System.Drawing.Size(519, 47);
            this._statesGroup.TabIndex = 3;
            this._statesGroup.TabStop = false;
            this._statesGroup.Text = "States";
            // 
            // _blendMode
            // 
            this._blendMode.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._blendMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._blendMode.FormattingEnabled = true;
            this._blendMode.Location = new System.Drawing.Point(175, 16);
            this._blendMode.Name = "_blendMode";
            this._blendMode.Size = new System.Drawing.Size(337, 21);
            this._blendMode.TabIndex = 2;
            // 
            // _wireframeGroup
            // 
            this._wireframeGroup.AutoSize = true;
            this._wireframeGroup.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._wireframeGroup.Location = new System.Drawing.Point(98, 20);
            this._wireframeGroup.Name = "_wireframeGroup";
            this._wireframeGroup.Size = new System.Drawing.Size(71, 17);
            this._wireframeGroup.TabIndex = 1;
            this._wireframeGroup.Text = "Wireframe";
            this._wireframeGroup.ThreeState = true;
            this._wireframeGroup.UseVisualStyleBackColor = true;
            // 
            // _doubleSidedCheck
            // 
            this._doubleSidedCheck.AutoSize = true;
            this._doubleSidedCheck.FlatStyle = System.Windows.Forms.FlatStyle.Flat;
            this._doubleSidedCheck.Location = new System.Drawing.Point(7, 20);
            this._doubleSidedCheck.Name = "_doubleSidedCheck";
            this._doubleSidedCheck.Size = new System.Drawing.Size(85, 17);
            this._doubleSidedCheck.TabIndex = 0;
            this._doubleSidedCheck.Text = "Double sided";
            this._doubleSidedCheck.ThreeState = true;
            this._doubleSidedCheck.UseVisualStyleBackColor = true;
            // 
            // _resourcesPage
            // 
            this._resourcesPage.Controls.Add(this._resourceBindings);
            this._resourcesPage.Location = new System.Drawing.Point(4, 22);
            this._resourcesPage.Name = "_resourcesPage";
            this._resourcesPage.Size = new System.Drawing.Size(517, 249);
            this._resourcesPage.TabIndex = 1;
            this._resourcesPage.Text = "Resources";
            this._resourcesPage.UseVisualStyleBackColor = true;
            // 
            // _shaderFlags
            // 
            this._shaderFlags.Controls.Add(this._materialParameterBox);
            this._shaderFlags.Location = new System.Drawing.Point(4, 22);
            this._shaderFlags.Name = "_shaderFlags";
            this._shaderFlags.Size = new System.Drawing.Size(517, 249);
            this._shaderFlags.TabIndex = 2;
            this._shaderFlags.Text = "Shader Flags";
            this._shaderFlags.UseVisualStyleBackColor = true;
            // 
            // MaterialControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._pages);
            this.Controls.Add(this._statesGroup);
            this.Name = "MaterialControl";
            this.Size = new System.Drawing.Size(525, 331);
            this._pages.ResumeLayout(false);
            this._constantsPage.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._shaderConstants)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._resourceBindings)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._materialParameterBox)).EndInit();
            this._statesGroup.ResumeLayout(false);
            this._statesGroup.PerformLayout();
            this._resourcesPage.ResumeLayout(false);
            this._shaderFlags.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox _statesGroup;
        private System.Windows.Forms.CheckBox _wireframeGroup;
        private System.Windows.Forms.CheckBox _doubleSidedCheck;
        private System.Windows.Forms.DataGridView _shaderConstants;
        private System.Windows.Forms.DataGridView _materialParameterBox;
        private System.Windows.Forms.DataGridView _resourceBindings;
        private System.Windows.Forms.TabControl _pages;
        private System.Windows.Forms.TabPage _constantsPage;
        private System.Windows.Forms.ComboBox _blendMode;
        private System.Windows.Forms.TabPage _resourcesPage;
        private System.Windows.Forms.TabPage _shaderFlags;
    }
}
