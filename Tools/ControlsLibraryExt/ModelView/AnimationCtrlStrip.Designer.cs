namespace ControlsLibraryExt
{
    partial class AnimationCtrlStrip
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
            this._animationSelection = new System.Windows.Forms.ComboBox();
            this._timeBar = new System.Windows.Forms.TrackBar();
            this._playButton = new System.Windows.Forms.CheckBox();
            ((System.ComponentModel.ISupportInitialize)(this._timeBar)).BeginInit();
            this.SuspendLayout();
            // 
            // _animationSelection
            // 
            this._animationSelection.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this._animationSelection.FormattingEnabled = true;
            this._animationSelection.Location = new System.Drawing.Point(4, 4);
            this._animationSelection.Name = "_animationSelection";
            this._animationSelection.Size = new System.Drawing.Size(168, 21);
            this._animationSelection.TabIndex = 0;
            this._animationSelection.SelectedIndexChanged += new System.EventHandler(this._animationSelection_SelectedIndexChanged);
            // 
            // _timeBar
            // 
            this._timeBar.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this._timeBar.Location = new System.Drawing.Point(178, 4);
            this._timeBar.Maximum = 1000000;
            this._timeBar.Name = "_timeBar";
            this._timeBar.Size = new System.Drawing.Size(483, 45);
            this._timeBar.TabIndex = 1;
            this._timeBar.TickStyle = System.Windows.Forms.TickStyle.None;
            this._timeBar.Scroll += new System.EventHandler(this._timeBar_Scroll);
            // 
            // _playButton
            // 
            this._playButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this._playButton.Appearance = System.Windows.Forms.Appearance.Button;
            this._playButton.AutoSize = true;
            this._playButton.Location = new System.Drawing.Point(667, 3);
            this._playButton.Name = "_playButton";
            this._playButton.Size = new System.Drawing.Size(37, 23);
            this._playButton.TabIndex = 2;
            this._playButton.Text = "Play";
            this._playButton.UseVisualStyleBackColor = true;
            this._playButton.CheckedChanged += new System.EventHandler(this.checkBox1_CheckedChanged);
            // 
            // AnimationCtrlStrip
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this._playButton);
            this.Controls.Add(this._timeBar);
            this.Controls.Add(this._animationSelection);
            this.Name = "AnimationCtrlStrip";
            this.Size = new System.Drawing.Size(707, 32);
            ((System.ComponentModel.ISupportInitialize)(this._timeBar)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ComboBox _animationSelection;
        private System.Windows.Forms.TrackBar _timeBar;
        private System.Windows.Forms.CheckBox _playButton;
    }
}
