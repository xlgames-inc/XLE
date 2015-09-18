using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Sce.Atf.Applications;

namespace LevelEditorXLE.Manipulators
{
    public partial class XLEScatterPlaceControls : UserControl
    {
        public XLEScatterPlaceControls()
        {
            InitializeComponent();
        }

        public IPropertyEditingContext Object
        {
            set { _properties.Bind(value); _attachedObject = value; }
        }

        public delegate void OnEvent(object obj, string fn);

        public OnEvent LoadEvent { get; set; }
        public OnEvent SaveEvent { get; set; }

        private void _loadModelList_Click(object sender, EventArgs e)
        {
            if (_ofd.ShowDialog() == DialogResult.OK)
                LoadEvent(_attachedObject, _ofd.FileName);
            _properties.RefreshProperties(); 
            _properties.Refresh();
        }

        private void _saveModelList_Click(object sender, EventArgs e)
        {
            if (_sfd.ShowDialog() == DialogResult.OK)
                SaveEvent(_attachedObject, _sfd.FileName);
        }

        private object _attachedObject = null;
        private SaveFileDialog _sfd = new SaveFileDialog();
        private OpenFileDialog _ofd = new OpenFileDialog();
    }
}
