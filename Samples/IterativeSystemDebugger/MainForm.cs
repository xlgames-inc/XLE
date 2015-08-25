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
            var system = new GUILayer.CFDRefIterativeSystem(32);
            var form = new RefCFD(system);
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
    }
}
