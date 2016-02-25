namespace LevelEditorXLE.Manipulators
{
    partial class XLEScatterPlaceControls
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
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this._properties = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
            this._saveModelList = new System.Windows.Forms.Button();
            this._loadModelList = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
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
            this.splitContainer1.Panel1.Controls.Add(this._properties);
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this._saveModelList);
            this.splitContainer1.Panel2.Controls.Add(this._loadModelList);
            this.splitContainer1.Panel2MinSize = 32;
            this.splitContainer1.Size = new System.Drawing.Size(313, 254);
            this.splitContainer1.SplitterDistance = 218;
            this.splitContainer1.TabIndex = 0;
            // 
            // _properties
            // 
            this._properties.Dock = System.Windows.Forms.DockStyle.Fill;
            this._properties.Font = new System.Drawing.Font("Segoe UI", 9F);
            this._properties.Location = new System.Drawing.Point(0, 0);
            this._properties.Name = "_properties";
            this._properties.PropertySorting = ((Sce.Atf.Controls.PropertyEditing.PropertySorting)(((Sce.Atf.Controls.PropertyEditing.PropertySorting.Categorized | Sce.Atf.Controls.PropertyEditing.PropertySorting.Alphabetical) 
            | Sce.Atf.Controls.PropertyEditing.PropertySorting.CategoryAlphabetical)));
            this._properties.SelectedPropertyDescriptor = null;
            this._properties.Settings = "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?><PropertyView PropertySort" +
    "ing=\"ByCategory\" />";
            this._properties.Size = new System.Drawing.Size(313, 218);
            this._properties.TabIndex = 0;
            // 
            // _saveModelList
            // 
            this._saveModelList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._saveModelList.Location = new System.Drawing.Point(200, 3);
            this._saveModelList.Name = "_saveModelList";
            this._saveModelList.Size = new System.Drawing.Size(106, 23);
            this._saveModelList.TabIndex = 1;
            this._saveModelList.Text = "Save Model List";
            this._saveModelList.UseVisualStyleBackColor = true;
            this._saveModelList.Click += new System.EventHandler(this._saveModelList_Click);
            // 
            // _loadModelList
            // 
            this._loadModelList.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._loadModelList.Location = new System.Drawing.Point(90, 3);
            this._loadModelList.Name = "_loadModelList";
            this._loadModelList.Size = new System.Drawing.Size(104, 23);
            this._loadModelList.TabIndex = 0;
            this._loadModelList.Text = "Load Model List";
            this._loadModelList.UseVisualStyleBackColor = true;
            this._loadModelList.Click += new System.EventHandler(this._loadModelList_Click);
            // 
            // XLEScatterPlaceControls
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.splitContainer1);
            this.Name = "XLEScatterPlaceControls";
            this.Size = new System.Drawing.Size(313, 254);
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.SplitContainer splitContainer1;
        private Sce.Atf.Controls.PropertyEditing.PropertyGrid _properties;
        private System.Windows.Forms.Button _saveModelList;
        private System.Windows.Forms.Button _loadModelList;
    }
}
