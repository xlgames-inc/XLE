// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
    public partial class InterfaceParameterForm : Form
    {
        public InterfaceParameterForm(bool showDeleteButton = true)
        {
            InitializeComponent();
            if (!showDeleteButton)
                _deleteButton.Hide();
        }

        public new string Name
        {
            get { return _ctrl.Name; }
            set { _ctrl.Name = value; }
        }
        
        public string Semantic
        {
            get { return _ctrl.Semantic; }
            set { _ctrl.Semantic = value; }
        }

        public string Default
        {
            get { return _ctrl.Default; }
            set { _ctrl.Default = value; }
        }

        public string Type
        {
            get { return _ctrl.Type; }
            set { _ctrl.Type = value; }
        }
    }
}
