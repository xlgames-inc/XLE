// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;
using System.Linq;

namespace MaterialTool
{
    public partial class DiagramSettings : Form
    {
        public DiagramSettings(NodeEditorCore.IDiagramDocument context)
        {
            InitializeComponent();
            _context = context;

            // We need to calculate the interface for the node graph and
            // find the variables from there...
            var interf = ShaderPatcherLayer.NodeGraph.GetInterface(context.NodeGraph);
            var sugg = interf.Variables.Select(x => x.Name);

            {
                _variables.AutoGenerateColumns = false;

                _variables.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        CellTemplate = new ControlsLibrary.VariableNameCell { Suggestions = sugg },
                        Name = "Variable",
                        HeaderText = "Variable",
                        ToolTipText = "Name of the variable",
                        DataPropertyName = "Key",
                        FillWeight = 30,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });

                _variables.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        CellTemplate = new ControlsLibrary.VariableRestrictionCell(),
                        Name = "Value",
                        HeaderText = "Restrictions",
                        ToolTipText = "Restrictions applied to the variable",
                        DataPropertyName = "Value",
                        FillWeight = 70,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });
            }

            _variablesList = new BindingList<ControlsLibrary.StringPair>();
            foreach (var i in _context.GraphContext.Variables)
                _variablesList.Add(new ControlsLibrary.StringPair { Key = i.Key, Value = i.Value });
            _variables.DataSource = _variablesList;

            var diagContext = _context.GraphContext;
            _type.SelectedIndex = diagContext.HasTechniqueConfig ? 1 : 0;
            _needsWorldPosition.CheckState = CheckState.Unchecked;
            foreach (var i in diagContext.ShaderParameters)
                if (i.Key.Equals("OUTPUT_WORLD_POSITION"))
                    _needsWorldPosition.CheckState = CheckState.Checked;

            _okButton.Click += _okButton_Click;
        }

        private void _okButton_Click(object sender, EventArgs e)
        {
            // commit the variables to the document
            _context.GraphContext.Variables.Clear();
            foreach (var i in _variablesList)
                _context.GraphContext.Variables.Add(i.Key ?? string.Empty, i.Value ?? string.Empty);

            var diagContext = _context.GraphContext;
            diagContext.HasTechniqueConfig = _type.SelectedIndex == 1;
            if (_needsWorldPosition.CheckState == CheckState.Checked)
                diagContext.ShaderParameters["OUTPUT_WORLD_POSITION"] = "1";
            else
                diagContext.ShaderParameters.Remove("OUTPUT_WORLD_POSITION");

            _context.Invalidate();
            DialogResult = DialogResult.OK;
        }

        private NodeEditorCore.IDiagramDocument _context;
        private BindingList<ControlsLibrary.StringPair> _variablesList;

        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            _needsWorldPosition.Enabled = _type.SelectedIndex == 1;
        }
    }
}
