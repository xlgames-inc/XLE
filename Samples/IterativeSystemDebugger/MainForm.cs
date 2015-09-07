using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace IterativeSystemDebugger
{
    public partial class MainForm : Form
    {
        public MainForm()
        {
            InitializeComponent();
            _forms = new List<Control>();
        }

        private void erosionSimToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var dlg = new OpenFileDialog();
            dlg.Filter = "Uber surface files (*.uber)|*.uber";
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                var system = new GUILayer.ErosionIterativeSystem(dlg.FileName);
                var form = new Erosion(system);
                form.Show();
                _forms.Add(form);
            }
        }

        private void referenceCFDToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var system = new GUILayer.CFDRefIterativeSystem(128);
            var form = new RefCFD(system, "RefCFDSettings");
            form.Show();
            _forms.Add(form);
        }

        private void customCFDToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var system = new GUILayer.CFDIterativeSystem(128);
            var form = new RefCFD(system, "RefCFDSettings");
            form.Show();
            _forms.Add(form);
        }

        private void customCFD3DToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var system = new GUILayer.CFD3DIterativeSystem(64, 64, 32);
            var form = new RefCFD(system, "RefCFDSettings");
            form.Show();
            _forms.Add(form);
        }

        private void cloudsForm2DToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var system = new GUILayer.CloudsIterativeSystem(128);
            var form = new RefCFD(system, "CloudsFormSettings");
            form.Show();
            _forms.Add(form);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            if (disposing && (_forms != null))
            {
                foreach (var f in _forms) f.Dispose();
            }
            base.Dispose(disposing);
        }

        IList<Control> _forms;

        private void invalidAssetsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            using (var dialog = new ControlsLibrary.InvalidAssetDialog())
                dialog.ShowDialog();
        }
                
    }
}
