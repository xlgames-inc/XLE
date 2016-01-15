using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ControlsLibrary.BasicControls
{
    public partial class TextWindow : Form
    {
        public TextWindow()
        {
            InitializeComponent();
        }

        public new string Text { set { _textBox.Text = value; } }
    }
}
