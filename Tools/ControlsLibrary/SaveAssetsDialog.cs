using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace ControlsLibrary
{
    public partial class SaveAssetsDialog : Form
    {
        public SaveAssetsDialog()
        {
            InitializeComponent();

            var originalValue = @"# Common material settings used by vegetation objects
# Enables double sided lighting and vegetation animation handling
Common
    ShaderParams ( MAT_VCOLOR_IS_ANIM_PARAM 1, MAT_DOUBLE_SIDED_LIGHTING 1, MAT_ALPHA_TEST 1 )
    States ( DoubleSided true )
    Constants ( Transmission 255 )
Branches
    ShaderParams ( MAT_VCOLOR_IS_ANIM_PARAM 1, MAT_ALPHA_TEST 1 )
    States ( DoubleSided true )";

            var newValue = @"# Common material settings used by vegetation objects
# Enables double sided lighting and vegetation animation handling
Common
    ShaderParams ( MAT_ALPHA_TEST 1 )
    Constants ( Transmission 128 )
Branches
";
            _compareWindow.Comparison = Tuple.Create(originalValue, newValue);

        }

        private void _saveButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.OK;
        }

        private void _cancelButton_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
        }
    }
}
