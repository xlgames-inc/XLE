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
        }

        private void erosionSimToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var dlg = new OpenFileDialog();
            dlg.Filter = "Uber surface files (*.uber)|*.uber";
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                var system = new GUILayer.ErosionIterativeSystem(dlg.FileName);
            }
        }
    }
}
