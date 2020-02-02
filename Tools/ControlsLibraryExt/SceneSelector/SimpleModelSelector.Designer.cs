namespace ControlsLibraryExt.SceneSelector
{
    partial class SimpleModelSelector
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
            this.model = new System.Windows.Forms.TextBox();
            this.material = new System.Windows.Forms.TextBox();
            this.skeleton = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.selectSkeleton = new System.Windows.Forms.Button();
            this.selectMaterial = new System.Windows.Forms.Button();
            this.selectModel = new System.Windows.Forms.Button();
            this.selectAnimation = new System.Windows.Forms.Button();
            this.label4 = new System.Windows.Forms.Label();
            this.animation = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // model
            // 
            this.model.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.model.Location = new System.Drawing.Point(75, 3);
            this.model.Name = "model";
            this.model.Size = new System.Drawing.Size(469, 20);
            this.model.TabIndex = 0;
            this.model.TextChanged += new System.EventHandler(this.model_TextChanged);
            // 
            // material
            // 
            this.material.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.material.Location = new System.Drawing.Point(75, 29);
            this.material.Name = "material";
            this.material.Size = new System.Drawing.Size(469, 20);
            this.material.TabIndex = 1;
            this.material.TextChanged += new System.EventHandler(this.material_TextChanged);
            // 
            // skeleton
            // 
            this.skeleton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.skeleton.Location = new System.Drawing.Point(75, 55);
            this.skeleton.Name = "skeleton";
            this.skeleton.Size = new System.Drawing.Size(469, 20);
            this.skeleton.TabIndex = 2;
            this.skeleton.TextChanged += new System.EventHandler(this.skeleton_TextChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(3, 10);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(36, 13);
            this.label1.TabIndex = 4;
            this.label1.Text = "Model";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(4, 35);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(44, 13);
            this.label2.TabIndex = 5;
            this.label2.Text = "Material";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(4, 61);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(49, 13);
            this.label3.TabIndex = 6;
            this.label3.Text = "Skeleton";
            // 
            // selectSkeleton
            // 
            this.selectSkeleton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.selectSkeleton.Location = new System.Drawing.Point(550, 55);
            this.selectSkeleton.Name = "selectSkeleton";
            this.selectSkeleton.Size = new System.Drawing.Size(25, 20);
            this.selectSkeleton.TabIndex = 7;
            this.selectSkeleton.Text = "...";
            this.selectSkeleton.UseVisualStyleBackColor = true;
            this.selectSkeleton.Click += new System.EventHandler(this.selectSkeleton_Click);
            // 
            // selectMaterial
            // 
            this.selectMaterial.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.selectMaterial.Location = new System.Drawing.Point(550, 29);
            this.selectMaterial.Name = "selectMaterial";
            this.selectMaterial.Size = new System.Drawing.Size(25, 20);
            this.selectMaterial.TabIndex = 8;
            this.selectMaterial.Text = "...";
            this.selectMaterial.UseVisualStyleBackColor = true;
            this.selectMaterial.Click += new System.EventHandler(this.selectMaterial_Click);
            // 
            // selectModel
            // 
            this.selectModel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.selectModel.Location = new System.Drawing.Point(550, 3);
            this.selectModel.Name = "selectModel";
            this.selectModel.Size = new System.Drawing.Size(25, 20);
            this.selectModel.TabIndex = 9;
            this.selectModel.Text = "...";
            this.selectModel.UseVisualStyleBackColor = true;
            this.selectModel.Click += new System.EventHandler(this.selectModel_Click);
            // 
            // selectAnimation
            // 
            this.selectAnimation.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.selectAnimation.Location = new System.Drawing.Point(550, 81);
            this.selectAnimation.Name = "selectAnimation";
            this.selectAnimation.Size = new System.Drawing.Size(25, 20);
            this.selectAnimation.TabIndex = 12;
            this.selectAnimation.Text = "...";
            this.selectAnimation.UseVisualStyleBackColor = true;
            this.selectAnimation.Click += new System.EventHandler(this.selectAnimation_Click);
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(4, 87);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(53, 13);
            this.label4.TabIndex = 11;
            this.label4.Text = "Animation";
            // 
            // animation
            // 
            this.animation.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.animation.Location = new System.Drawing.Point(75, 81);
            this.animation.Name = "animation";
            this.animation.Size = new System.Drawing.Size(469, 20);
            this.animation.TabIndex = 10;
            this.animation.TextChanged += new System.EventHandler(this.animation_TextChanged);
            // 
            // SimpleModelSelector
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.selectAnimation);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.animation);
            this.Controls.Add(this.selectModel);
            this.Controls.Add(this.selectMaterial);
            this.Controls.Add(this.selectSkeleton);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.skeleton);
            this.Controls.Add(this.material);
            this.Controls.Add(this.model);
            this.Name = "SimpleModelSelector";
            this.Size = new System.Drawing.Size(578, 106);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox model;
        private System.Windows.Forms.TextBox material;
        private System.Windows.Forms.TextBox skeleton;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Button selectSkeleton;
        private System.Windows.Forms.Button selectMaterial;
        private System.Windows.Forms.Button selectModel;
        private System.Windows.Forms.Button selectAnimation;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.TextBox animation;
    }
}
