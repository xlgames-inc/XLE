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

        static public void ShowModal(String text)
        {
            using (var dlg = new TextWindow())
            {
                dlg.Text = text;
                dlg.ShowDialog();
            }
        }

        static public void Show(String text)
        {
            new TextWindow() { Text = text }.Show();
        }

        public new string Text { set { _textBox.Text = value; } }
    }
}
