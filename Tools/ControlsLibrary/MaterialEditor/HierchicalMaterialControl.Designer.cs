// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace ControlsLibrary.MaterialEditor
{
    partial class HierchicalMaterialControl
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
                if (components != null)
                    components.Dispose();
                ClearComboBoxNodes();
                _materialControl.Dispose();
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
            this._hierachyTree = new ComboTreeBox();
            this._materialControl = new MaterialControl();
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer1)).BeginInit();
            this._splitContainer1.Panel1.SuspendLayout();
            this._splitContainer1.Panel2.SuspendLayout();
            this._splitContainer1.SuspendLayout();
            this.SuspendLayout();
            // 
            // splitContainer1
            // 
            this._splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this._splitContainer1.Location = new System.Drawing.Point(0, 0);
            this._splitContainer1.Name = "splitContainer1";
            this._splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
            this._splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this._splitContainer1.IsSplitterFixed = true;
            // 
            // splitContainer1.Panel1
            // 
            this._splitContainer1.Panel1.Controls.Add(this._hierachyTree);
            this._splitContainer1.Panel1.RightToLeft = System.Windows.Forms.RightToLeft.No;
            // 
            // splitContainer1.Panel2
            // 
            this._splitContainer1.Panel2.Controls.Add(this._materialControl);
            this._splitContainer1.Panel2.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this._splitContainer1.Size = new System.Drawing.Size(326, 607);
            this._splitContainer1.SplitterDistance = 24;
            this._splitContainer1.TabIndex = 0;
            // 
            // treeView1
            // 
            // this.treeView1.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this._hierachyTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this._hierachyTree.Location = new System.Drawing.Point(0, 0);
            this._hierachyTree.Name = "treeView1";
            this._hierachyTree.Size = new System.Drawing.Size(326, 58);
            this._hierachyTree.TabIndex = 0;
            // 
            // materialControl1
            // 
            this._materialControl.Dock = System.Windows.Forms.DockStyle.Fill;
            this._materialControl.Location = new System.Drawing.Point(0, 0);
            this._materialControl.Name = "materialControl1";
            this._materialControl.Size = new System.Drawing.Size(721, 545);
            this._materialControl.TabIndex = 0;
            // 
            // HierchicalMaterialControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._splitContainer1);
            this.Name = "HierchicalMaterialControl";
            this.Size = new System.Drawing.Size(326, 607);
            this._splitContainer1.Panel1.ResumeLayout(false);
            this._splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this._splitContainer1)).EndInit();
            this._splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer _splitContainer1;
        private ComboTreeBox _hierachyTree;
        private MaterialControl _materialControl;
    }
}
