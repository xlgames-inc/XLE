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
    public partial class ExceptionReport : Form
    {
        public ExceptionReport()
        {
            InitializeComponent();
        }

        public string While { set { _while.Text = value; } }
        public string Type { set { _type.Text = value; } }
        public string Message { set { _message.Text = value; } }
        public string Callstack { set { _callstack.Text = value; } }

        static public void Show(Exception e, string whileMessage)
        {
            using (var dlg =
                new ExceptionReport()
                { While = whileMessage, Message = e.Message, Callstack = e.StackTrace, Type = e.GetType().FullName })
            {
                dlg.ShowDialog();
            }
        }
    }
}
