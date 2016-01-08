// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

namespace NodeEditorCore
{
    partial class InputParameterControl
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
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this._nameBox = new System.Windows.Forms.TextBox();
            this._semanticBox = new System.Windows.Forms.TextBox();
            this.label4 = new System.Windows.Forms.Label();
            this._typeBaseBox = new System.Windows.Forms.ComboBox();
            this._typeDimension0Box = new System.Windows.Forms.ComboBox();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(40, 4);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(31, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Type";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(193, 4);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(35, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Name";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(347, 4);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(51, 13);
            this.label3.TabIndex = 2;
            this.label3.Text = "Semantic";
            // 
            // _nameBox
            // 
            this._nameBox.Location = new System.Drawing.Point(149, 21);
            this._nameBox.Name = "_nameBox";
            this._nameBox.Size = new System.Drawing.Size(133, 20);
            this._nameBox.TabIndex = 4;
            this._nameBox.TextChanged += new System.EventHandler(this._name_TextChanged);
            // 
            // _semanticBox
            // 
            this._semanticBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._semanticBox.Location = new System.Drawing.Point(304, 21);
            this._semanticBox.Name = "_semanticBox";
            this._semanticBox.Size = new System.Drawing.Size(179, 20);
            this._semanticBox.TabIndex = 5;
            this._semanticBox.TextChanged += new System.EventHandler(this._semanticBox_TextChanged);
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(288, 24);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(10, 13);
            this.label4.TabIndex = 6;
            this.label4.Text = ":";
            // 
            // _typeBaseBox
            // 
            this._typeBaseBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._typeBaseBox.FormattingEnabled = true;
            this._typeBaseBox.Location = new System.Drawing.Point(3, 21);
            this._typeBaseBox.Name = "_typeBaseBox";
            this._typeBaseBox.Size = new System.Drawing.Size(96, 21);
            this._typeBaseBox.TabIndex = 7;
            this._typeBaseBox.SelectedIndexChanged += new System.EventHandler(this._typeBox_SelectedIndexChanged);
            // 
            // _typeDimension0Box
            // 
            this._typeDimension0Box.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._typeDimension0Box.FormattingEnabled = true;
            this._typeDimension0Box.Location = new System.Drawing.Point(105, 21);
            this._typeDimension0Box.Name = "_typeDimension0Box";
            this._typeDimension0Box.Size = new System.Drawing.Size(38, 21);
            this._typeDimension0Box.TabIndex = 8;
            this._typeDimension0Box.SelectedIndexChanged += new System.EventHandler(this._typeBox_SelectedIndexChanged);
            // 
            // InputParameterControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._typeDimension0Box);
            this.Controls.Add(this._typeBaseBox);
            this.Controls.Add(this.label4);
            this.Controls.Add(this._semanticBox);
            this.Controls.Add(this._nameBox);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Name = "InputParameterControl";
            this.Size = new System.Drawing.Size(486, 48);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.TextBox _nameBox;
        private System.Windows.Forms.TextBox _semanticBox;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.ComboBox _typeBaseBox;
        private System.Windows.Forms.ComboBox _typeDimension0Box;
    }
}
