// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NodeEditorCore
{
    public partial class InterfaceParameterControl : UserControl
    {
        enum KnownBaseTypes { auto, @float, @int, @uint, Texture2D };

        public InterfaceParameterControl()
        {
            InitializeComponent();

            _typeBaseBox.Items.AddRange(System.Enum.GetNames(typeof(KnownBaseTypes)));
            _typeDimension0Box.Items.AddRange(new string[] { "1", "2", "3", "4" });
        }

        public new string Name
        {
            get { return _name; }
            set { _name = value; _nameBox.Text = value; }
        }
        
        public string Semantic
        {
            get { return _semantic; }
            set { _semantic = value; _semanticBox.Text = value; }
        }

        public string Default
        {
            get { return _default; }
            set { _default = value; _defaultBox.Text = value; }
        }

        public string Type
        {
            get { return _type; }
            set
            {
                _type = value;

                // We need to parse the type name and convert it into something we will understand.
                // We actually understand only a small range of typenames: 
                //  "float\d?" or "int\d?" or "uint\d?" or "Texture2D"
                // Maybe Texture2D<??> could be useful...?
                if (string.IsNullOrEmpty(_type))
                {
                    _typeBaseBox.SelectedIndex = (int)KnownBaseTypes.auto;
                    _typeDimension0Box.Enabled = false;
                }
                else if (_type == "Texture2D")
                {
                    _typeBaseBox.SelectedIndex = (int)KnownBaseTypes.Texture2D;
                    _typeDimension0Box.Enabled = false;
                }
                else
                {
                    var breakdown = new ShaderPatcherLayer.TypeRules.TypeBreakdown(_type);
                    var parsedType = System.Enum.Parse(typeof(KnownBaseTypes), breakdown.RawType);
                    KnownBaseTypes t = (parsedType == null) ? KnownBaseTypes.@float : (KnownBaseTypes)parsedType;
                    _typeBaseBox.SelectedIndex = (int)t;
                    _typeDimension0Box.Enabled = true;
                    _typeDimension0Box.SelectedIndex = Math.Max(1,breakdown.Dimension0)-1;
                }
            }
        }

        private void _typeBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            KnownBaseTypes t = (KnownBaseTypes)_typeBaseBox.SelectedIndex;
            if (t == KnownBaseTypes.auto)
            {
                _typeDimension0Box.Enabled = false;
                _type = "auto";
            } 
            else if (t == KnownBaseTypes.Texture2D)
            {
                _typeDimension0Box.Enabled = false;
                _type = t.ToString();
            }
            else
            {
                _typeDimension0Box.Enabled = true;
                _type = t.ToString();
                var count = _typeDimension0Box.SelectedIndex + 1;
                if (count > 1)
                    _type += count; // append the count to the string
            }
        }

        private void _name_TextChanged(object sender, EventArgs e) { _name = _nameBox.Text; }
        private void _semanticBox_TextChanged(object sender, EventArgs e) { _semantic = _semanticBox.Text; }
        private void _defaultBox_TextChanged(object sender, EventArgs e) { _default = _defaultBox.Text; }
        private string _name, _type, _semantic, _default;
    }
}

