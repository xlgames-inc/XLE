// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
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
                // note --  We can get "PendingAsset" exceptions here!
                //          we need some way to handle these properly
                _matControls.Object = value;
                if (_preview != null)
                {
                    _preview.Object = value;
                    _preview.RawMaterialList = _matControls.RawMaterialList;
                }
            }
        }

        public Tuple<string, ulong> PreviewModel
        {
            set
            {
                if (_preview != null)
                    _preview.PreviewModel = value;
            }
        }
    }
}
