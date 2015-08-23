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
    public partial class Erosion : Form
    {
        public Erosion(GUILayer.ErosionIterativeSystem system)
        {
            _system = system;
            InitializeComponent();
            _previewWindow.Underlying.AddSystem(_system._overlay);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            if (_system != null) { _system.Dispose(); _system = null; }
            if (_previewWindow!=null) { _previewWindow.Dispose(); }
            base.Dispose(disposing);
        }

        private GUILayer.ErosionIterativeSystem _system;
    }
}
