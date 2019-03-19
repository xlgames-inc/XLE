using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NodeEditorCore
{
    public partial class SubGraphPropertiesForm : Form
    {
        public SubGraphPropertiesForm()
        {
            InitializeComponent();
        }

        public string Name
        {
            get { return _nameBox.Text; }
            set { _nameBox.Text = value; }
        }

        public string Implements
        {
            get { return _implementsBox.Text; }
            set { _implementsBox.Text = value; }
        }
    }
}
