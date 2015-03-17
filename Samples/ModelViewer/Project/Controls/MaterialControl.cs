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

namespace ModelViewer.Controls
{
    public partial class MaterialControl : UserControl
    {
        public MaterialControl()
        {
            InitializeComponent();

            //  Create the columns programmatically (because the
            //  automatic generation seems to get the columns in
            //  the wrong order).
            {
                materialParameterBox.AutoGenerateColumns = false;
                // var cell = new DataGridViewTextBoxCell();

                materialParameterBox.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "Name",
                        HeaderText = "Name",
                        ToolTipText = "Name of the property",
                        DataPropertyName = "Name",
                        FillWeight = 70,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });

                materialParameterBox.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "Value",
                        HeaderText = "Value",
                        ToolTipText = "Integer value",
                        DataPropertyName = "Value",
                        FillWeight = 30,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });
            }

            {
                shaderConstants.AutoGenerateColumns = false;
                // var cell = new DataGridViewTextBoxCell();

                shaderConstants.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "Name",
                        HeaderText = "Name",
                        ToolTipText = "Name of the property",
                        DataPropertyName = "Name",
                        FillWeight = 70,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });

                shaderConstants.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "Value",
                        HeaderText = "Value",
                        ToolTipText = "Integer value",
                        DataPropertyName = "Value",
                        FillWeight = 30,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });
            }
        }

        public GUILayer.RawMaterial Object
        {
            set
            {
                materialParameterBox.DataSource = value.MaterialParameterBox;
                shaderConstants.DataSource = value.ShaderConstants;
                materialPreview1.Object = value;
            }
        }
    }
}
