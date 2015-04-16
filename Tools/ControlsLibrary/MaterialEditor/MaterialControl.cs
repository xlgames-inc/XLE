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

namespace ControlsLibrary.MaterialEditor
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

            {
                resourceBindings.AutoGenerateColumns = false;
                // var cell = new DataGridViewTextBoxCell();

                resourceBindings.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "Binding",
                        HeaderText = "Binding",
                        ToolTipText = "Binding point in the shader",
                        DataPropertyName = "Name",
                        FillWeight = 30,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });

                resourceBindings.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        // CellTemplate = cell,
                        Name = "ResourceName",
                        HeaderText = "ResourceName",
                        ToolTipText = "Name of the bound resourec",
                        DataPropertyName = "Value",
                        FillWeight = 70,
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
                resourceBindings.DataSource = value.ResourceBindings;
                materialPreview1.Object = value;

                checkBox1.DataBindings.Clear();
                checkBox2.DataBindings.Clear();
                if (value.StateSet != null) 
                {
                    checkBox1.DataBindings.Add("CheckState", value.StateSet, "DoubleSided", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged);
                    checkBox2.DataBindings.Add("CheckState", value.StateSet, "Wireframe", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged);
                }
                else
                {
                    checkBox1.CheckState = CheckState.Indeterminate;
                    checkBox2.CheckState = CheckState.Indeterminate;
                }
            }
        }
    }
}
