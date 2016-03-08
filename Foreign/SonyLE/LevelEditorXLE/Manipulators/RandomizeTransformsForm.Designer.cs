namespace LevelEditorXLE.Manipulators
{
    partial class RandomizeTransformsForm
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
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this._scaleStdDev = new System.Windows.Forms.NumericUpDown();
            this._scaleMean = new System.Windows.Forms.NumericUpDown();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this._scaleMode = new System.Windows.Forms.ComboBox();
            this._scaleMaximum = new System.Windows.Forms.NumericUpDown();
            this._scaleMinimum = new System.Windows.Forms.NumericUpDown();
            this.label2 = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this._okButton = new System.Windows.Forms.Button();
            this._cancelButton = new System.Windows.Forms.Button();
            this._enableRotation = new System.Windows.Forms.CheckBox();
            this.groupBox1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this._scaleStdDev)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMean)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMaximum)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMinimum)).BeginInit();
            this.SuspendLayout();
            // 
            // groupBox1
            // 
            this.groupBox1.Controls.Add(this._scaleStdDev);
            this.groupBox1.Controls.Add(this._scaleMean);
            this.groupBox1.Controls.Add(this.label3);
            this.groupBox1.Controls.Add(this.label4);
            this.groupBox1.Controls.Add(this._scaleMode);
            this.groupBox1.Controls.Add(this._scaleMaximum);
            this.groupBox1.Controls.Add(this._scaleMinimum);
            this.groupBox1.Controls.Add(this.label2);
            this.groupBox1.Controls.Add(this.label1);
            this.groupBox1.Location = new System.Drawing.Point(12, 12);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(260, 152);
            this.groupBox1.TabIndex = 1;
            this.groupBox1.TabStop = false;
            this.groupBox1.Text = "Scales";
            // 
            // _scaleStdDev
            // 
            this._scaleStdDev.DecimalPlaces = 2;
            this._scaleStdDev.Increment = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this._scaleStdDev.Location = new System.Drawing.Point(133, 112);
            this._scaleStdDev.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this._scaleStdDev.Name = "_scaleStdDev";
            this._scaleStdDev.Size = new System.Drawing.Size(120, 20);
            this._scaleStdDev.TabIndex = 10;
            this._scaleStdDev.Value = new decimal(new int[] {
            5,
            0,
            0,
            65536});
            // 
            // _scaleMean
            // 
            this._scaleMean.DecimalPlaces = 2;
            this._scaleMean.Increment = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this._scaleMean.Location = new System.Drawing.Point(6, 112);
            this._scaleMean.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this._scaleMean.Name = "_scaleMean";
            this._scaleMean.Size = new System.Drawing.Size(120, 20);
            this._scaleMean.TabIndex = 7;
            this._scaleMean.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(133, 96);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(43, 13);
            this.label3.TabIndex = 9;
            this.label3.Text = "StdDev";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(7, 96);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(34, 13);
            this.label4.TabIndex = 8;
            this.label4.Text = "Mean";
            // 
            // _scaleMode
            // 
            this._scaleMode.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._scaleMode.FormattingEnabled = true;
            this._scaleMode.Items.AddRange(new object[] {
            "None",
            "Uniform",
            "Normal"});
            this._scaleMode.Location = new System.Drawing.Point(6, 19);
            this._scaleMode.Name = "_scaleMode";
            this._scaleMode.Size = new System.Drawing.Size(247, 21);
            this._scaleMode.TabIndex = 6;
            this._scaleMode.SelectedIndexChanged += new System.EventHandler(this._scaleMode_SelectedIndexChanged);
            // 
            // _scaleMaximum
            // 
            this._scaleMaximum.DecimalPlaces = 2;
            this._scaleMaximum.Increment = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this._scaleMaximum.Location = new System.Drawing.Point(133, 69);
            this._scaleMaximum.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this._scaleMaximum.Name = "_scaleMaximum";
            this._scaleMaximum.Size = new System.Drawing.Size(120, 20);
            this._scaleMaximum.TabIndex = 5;
            this._scaleMaximum.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            // 
            // _scaleMinimum
            // 
            this._scaleMinimum.DecimalPlaces = 2;
            this._scaleMinimum.Increment = new decimal(new int[] {
            1,
            0,
            0,
            65536});
            this._scaleMinimum.Location = new System.Drawing.Point(6, 69);
            this._scaleMinimum.Minimum = new decimal(new int[] {
            1,
            0,
            0,
            131072});
            this._scaleMinimum.Name = "_scaleMinimum";
            this._scaleMinimum.Size = new System.Drawing.Size(120, 20);
            this._scaleMinimum.TabIndex = 2;
            this._scaleMinimum.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(133, 53);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(27, 13);
            this.label2.TabIndex = 4;
            this.label2.Text = "Max";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(7, 53);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(24, 13);
            this.label1.TabIndex = 3;
            this.label1.Text = "Min";
            // 
            // _okButton
            // 
            this._okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._okButton.Location = new System.Drawing.Point(11, 193);
            this._okButton.Name = "_okButton";
            this._okButton.Size = new System.Drawing.Size(94, 23);
            this._okButton.TabIndex = 2;
            this._okButton.Text = "Ok";
            this._okButton.UseVisualStyleBackColor = true;
            // 
            // _cancelButton
            // 
            this._cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this._cancelButton.Location = new System.Drawing.Point(111, 193);
            this._cancelButton.Name = "_cancelButton";
            this._cancelButton.Size = new System.Drawing.Size(94, 23);
            this._cancelButton.TabIndex = 3;
            this._cancelButton.Text = "Cancel";
            this._cancelButton.UseVisualStyleBackColor = true;
            // 
            // _enableRotation
            // 
            this._enableRotation.AutoSize = true;
            this._enableRotation.Location = new System.Drawing.Point(11, 170);
            this._enableRotation.Name = "_enableRotation";
            this._enableRotation.Size = new System.Drawing.Size(127, 17);
            this._enableRotation.TabIndex = 4;
            this._enableRotation.Text = "Randomize Rotations";
            this._enableRotation.UseVisualStyleBackColor = true;
            // 
            // RandomizeTransformsForm
            // 
            this.AcceptButton = this._okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this._cancelButton;
            this.ClientSize = new System.Drawing.Size(282, 228);
            this.Controls.Add(this._enableRotation);
            this.Controls.Add(this._cancelButton);
            this.Controls.Add(this._okButton);
            this.Controls.Add(this.groupBox1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Name = "RandomizeTransformsForm";
            this.Text = "RandomizeTransformsForm";
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this._scaleStdDev)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMean)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMaximum)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this._scaleMinimum)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.NumericUpDown _scaleMaximum;
        private System.Windows.Forms.NumericUpDown _scaleMinimum;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button _okButton;
        private System.Windows.Forms.Button _cancelButton;
        private System.Windows.Forms.CheckBox _enableRotation;
        private System.Windows.Forms.NumericUpDown _scaleStdDev;
        private System.Windows.Forms.NumericUpDown _scaleMean;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.ComboBox _scaleMode;
    }
}