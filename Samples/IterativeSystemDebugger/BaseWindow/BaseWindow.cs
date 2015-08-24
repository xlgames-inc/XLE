using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using Sce.Atf.Applications;

namespace IterativeSystemDebugger
{
    public partial class BaseWindow : Form
    {
        public BaseWindow()
        {
            _autoTickState = false;
            InitializeComponent();
        }

        protected override void Dispose(bool disposing)
        {
            Application.Idle -= OnAppIdle;
            if (disposing && (components != null))
            {
                components.Dispose();
            }
			if (_previewWindow != null) { _previewWindow.Dispose(); _previewWindow = null; }
            base.Dispose(disposing);
        }

        protected virtual void DoTick() { }

        private void _tickButton_Click(object sender, EventArgs e)
        {
            DoTick();
            _previewWindow.Invalidate();
        }

        private void OnAppIdle(object sender, EventArgs e)
        {
            DoTick();
            _previewWindow.Invalidate();
        }

        private void _autoTick_CheckedChanged(object sender, EventArgs e)
        {
            bool newAutoTickState = _autoTick.CheckState == CheckState.Checked;
            if (newAutoTickState != _autoTickState) {
                _autoTickState = newAutoTickState;
                if (_autoTickState)
                {
                    Application.Idle += OnAppIdle;
                }
                else
                {
                    Application.Idle -= OnAppIdle;
                }
            }
        }

        private bool _autoTickState;
    }
}
