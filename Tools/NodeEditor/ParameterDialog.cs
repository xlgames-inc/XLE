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
using System.Reflection;

namespace NodeEditor
{
    public partial class ParameterDialog : Form
    {
        public ParameterDialog()
        {
            InitializeComponent();
            _activeParameter = new ShaderFragmentArchive.Parameter("");
            propertyGrid1.SelectedObject = _activeParameter;
        }

        public void PullFrom(ShaderFragmentArchive.Parameter parameter)
        {
            _activeParameter.DeepCopyFrom(parameter);
        }

        public void PushTo(ShaderFragmentArchive.Parameter destination)
        {
            destination.DeepCopyFrom(_activeParameter);
        }

        public ShaderFragmentArchive.Parameter Result 
        { 
            get { return _activeParameter; } 
        }

        private ShaderFragmentArchive.Parameter _activeParameter;

        private void okButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
            Close();
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }
    }
}
