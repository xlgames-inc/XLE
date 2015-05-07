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

        protected List<GUILayer.RawMaterial> _objectsPendingDispose = null;
        protected GUILayer.RawMaterial _currentFocusMat = null;

        protected void ClearAndDispose()
        {
            materialParameterBox.DataSource = null;
            shaderConstants.DataSource =  null;
            resourceBindings.DataSource = null;
            materialPreview1.Object = null;
            foreach (var o in _controls) o.Object = null;
            checkBox1.DataBindings.Clear();
            checkBox2.DataBindings.Clear();
            checkBox1.CheckState = CheckState.Indeterminate;
            checkBox2.CheckState = CheckState.Indeterminate;

            if (_currentFocusMat != null)
            {
                    // unbind our callbacks... We don't need to leave behind redundant callbacks everywhere
                _currentFocusMat.MaterialParameterBox.ListChanged -= OnParameterChanged;
                _currentFocusMat.ShaderConstants.ListChanged -= OnParameterChanged;
                _currentFocusMat.ResourceBindings.ListChanged -= OnParameterChanged;
                _currentFocusMat = null;
            }

            if (_objectsPendingDispose != null)
            {
                foreach (var mat in _objectsPendingDispose)
                    mat.Dispose();
                _objectsPendingDispose.Clear();
            }
        }

        public IEnumerable<string> Object
        {
            set
            {
                ClearAndDispose();

                var mats = new List<GUILayer.RawMaterial>();
                if (value != null)
                    foreach (var s in value)
                        mats.Add(new GUILayer.RawMaterial(s));

                materialPreview1.Object = mats; 
                _objectsPendingDispose = mats;

                var focusMat = (mats.Count > 0) ? mats[mats.Count-1] : null;
                if (focusMat != null)
                {
                    foreach (var o in _controls) o.Object = focusMat;
                    materialParameterBox.DataSource = focusMat.MaterialParameterBox;
                    shaderConstants.DataSource = focusMat.ShaderConstants;
                    resourceBindings.DataSource = focusMat.ResourceBindings;

                    if (focusMat.StateSet != null)
                    {
                        checkBox1.DataBindings.Add("CheckState", focusMat.StateSet, "DoubleSided", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged);
                        checkBox2.DataBindings.Add("CheckState", focusMat.StateSet, "Wireframe", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged);
                    }

                    focusMat.MaterialParameterBox.ListChanged += OnParameterChanged;
                    focusMat.ShaderConstants.ListChanged += OnParameterChanged;
                    focusMat.ResourceBindings.ListChanged += OnParameterChanged;
                    _currentFocusMat = focusMat;
                }
            }
        }

        void OnParameterChanged(object sender, ListChangedEventArgs e)
        {
            materialPreview1.InvalidatePreview();
        }

        public Tuple<string, ulong> PreviewModel
        {
            set { materialPreview1.PreviewModel = value; }
        }

        public GUILayer.EnvironmentSettingsSet EnvironmentSet
        {
            set { materialPreview1.EnvironmentSet = value; }
        }

        public abstract class ExtraControls : UserControl
        {
            public abstract GUILayer.RawMaterial Object { set; }
        }

        protected IList<ExtraControls> _controls = new List<ExtraControls>();

        public void AddExtraControls(string name, ExtraControls c)
        {
            _controls.Add(c);
            var page = new TabPage(name);
            page.Padding = new System.Windows.Forms.Padding(3);
            page.UseVisualStyleBackColor = true;
            page.Controls.Add(c);
            _pages.TabPages.Add(page);
            _pages.SelectedTab = page;
        }
    }
}
