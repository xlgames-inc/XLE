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

namespace ModelViewer
{
    public partial class ModalMaterialEditor : Form
    {
        public ModalMaterialEditor()
        {
            InitializeComponent();
        }

        public string Object
        {
            set
            {
                hierchicalMaterialControl1.Object = value;
            }
        }
    }
}
