using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace HyperGraph
{
	public sealed partial class TextEditForm : Form
	{
		public TextEditForm()
		{
			InitializeComponent();
		}

		public string InputText { get { return TextTextBox.Text; } set { TextTextBox.Text = value; } }
	}
}
