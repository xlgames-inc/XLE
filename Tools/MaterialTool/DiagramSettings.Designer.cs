namespace MaterialTool
{
    partial class DiagramSettings
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
            this._okButton = new System.Windows.Forms.Button();
            this._variables = new System.Windows.Forms.DataGridView();
            this.label1 = new System.Windows.Forms.Label();
            this._type = new System.Windows.Forms.ComboBox();
            this.label2 = new System.Windows.Forms.Label();
            this._needsWorldPosition = new System.Windows.Forms.CheckBox();
            ((System.ComponentModel.ISupportInitialize)(this._variables)).BeginInit();
            this.SuspendLayout();
            // 
            // _okButton
            // 
            this._okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this._okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this._okButton.Location = new System.Drawing.Point(557, 235);
            this._okButton.Margin = new System.Windows.Forms.Padding(2);
            this._okButton.Name = "_okButton";
            this._okButton.Size = new System.Drawing.Size(104, 26);
            this._okButton.TabIndex = 0;
            this._okButton.Text = "Ok";
            this._okButton.UseVisualStyleBackColor = true;
            // 
            // _variables
            // 
            this._variables.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._variables.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this._variables.Location = new System.Drawing.Point(11, 56);
            this._variables.Margin = new System.Windows.Forms.Padding(2);
            this._variables.Name = "_variables";
            this._variables.RowTemplate.Height = 24;
            this._variables.Size = new System.Drawing.Size(650, 174);
            this._variables.TabIndex = 1;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(9, 40);
            this.label1.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(50, 13);
            this.label1.TabIndex = 2;
            this.label1.Text = "Variables";
            // 
            // _type
            // 
            this._type.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._type.FormattingEnabled = true;
            this._type.Items.AddRange(new object[] {
            "Shader Function",
            "Technique (Material for an object)"});
            this._type.Location = new System.Drawing.Point(86, 8);
            this._type.Margin = new System.Windows.Forms.Padding(2);
            this._type.Name = "_type";
            this._type.Size = new System.Drawing.Size(186, 21);
            this._type.TabIndex = 3;
            this._type.SelectedIndexChanged += new System.EventHandler(this.comboBox1_SelectedIndexChanged);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(9, 11);
            this.label2.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(73, 13);
            this.label2.TabIndex = 4;
            this.label2.Text = "Diagram Type";
            // 
            // _needsWorldPosition
            // 
            this._needsWorldPosition.AutoSize = true;
            this._needsWorldPosition.Location = new System.Drawing.Point(528, 10);
            this._needsWorldPosition.Name = "_needsWorldPosition";
            this._needsWorldPosition.Size = new System.Drawing.Size(128, 17);
            this._needsWorldPosition.TabIndex = 5;
            this._needsWorldPosition.Text = "Needs World Position";
            this._needsWorldPosition.UseVisualStyleBackColor = true;
            // 
            // DiagramSettings
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(668, 267);
            this.Controls.Add(this._needsWorldPosition);
            this.Controls.Add(this.label2);
            this.Controls.Add(this._type);
            this.Controls.Add(this.label1);
            this.Controls.Add(this._variables);
            this.Controls.Add(this._okButton);
            this.Margin = new System.Windows.Forms.Padding(2);
            this.Name = "DiagramSettings";
            this.Text = "DiagramSettings";
            ((System.ComponentModel.ISupportInitialize)(this._variables)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button _okButton;
        private System.Windows.Forms.DataGridView _variables;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.ComboBox _type;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.CheckBox _needsWorldPosition;
    }
}