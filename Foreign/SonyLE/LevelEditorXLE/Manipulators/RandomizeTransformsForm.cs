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

namespace LevelEditorXLE.Manipulators
{
    public partial class RandomizeTransformsForm : Form
    {
        public RandomizeTransformsForm()
        {
            InitializeComponent();
            _scaleMode.SelectedIndex = 0;
        }

        public class Settings
        {
            public enum RandomizeMode { None, Uniform, Normal }
            public RandomizeMode RandomizeScales { get; set; }
            public bool RandomizeRotations { get; set; }
            public float ScaleMinimum { get; set; }
            public float ScaleMaximum { get; set; }
            public float ScaleMean { get; set; }
            public float ScaleStdDev { get; set; }
            public Settings() 
            {
                RandomizeScales = RandomizeMode.None;
                RandomizeRotations = false;
                ScaleMinimum = ScaleMaximum = 1.0f;
                ScaleMean = 1.0f; ScaleStdDev = 0.5f;
            }
        }

        public Settings Value
        {
            get
            {
                return new Settings
                    {
                        RandomizeScales = (Settings.RandomizeMode)_scaleMode.SelectedIndex,
                        RandomizeRotations = _enableRotation.Checked,
                        ScaleMinimum = Decimal.ToSingle(_scaleMinimum.Value),
                        ScaleMaximum = Decimal.ToSingle(_scaleMaximum.Value),
                        ScaleMean = Decimal.ToSingle(_scaleMean.Value),
                        ScaleStdDev = Decimal.ToSingle(_scaleStdDev.Value)
                    };
            }
        }

        private void _scaleMode_SelectedIndexChanged(object sender, EventArgs e)
        {
            _scaleMinimum.Enabled = (_scaleMode.SelectedIndex == 1);
            _scaleMaximum.Enabled = (_scaleMode.SelectedIndex == 1);
            _scaleMean.Enabled = (_scaleMode.SelectedIndex == 2);
            _scaleStdDev.Enabled = (_scaleMode.SelectedIndex == 2);
        }
    }
}
