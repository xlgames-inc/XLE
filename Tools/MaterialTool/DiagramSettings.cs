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
///////////////////////////////////////////////////////////////////////////////////////////////////

    internal class VariableNameEditControl : ComboBox, IDataGridViewEditingControl
    {
        public VariableNameEditControl()
        {
            this.TextChanged += (sender, e) => { OnValueChanged(e); };
            this.SelectedIndexChanged += (sender, e) => { OnValueChanged(e); };
        }

        // Implements the IDataGridViewEditingControl.EditingControlFormattedValue  
        // property. 
        public object EditingControlFormattedValue
        {
            get
            {
                return Value;
            }

            set
            {
                Value = value.ToString();
            }
        }

        internal string Value
        {
            get { return Text; }
            set { Text = value; }
        }

        // Implements the  
        // IDataGridViewEditingControl.GetEditingControlFormattedValue method. 
        public object GetEditingControlFormattedValue(DataGridViewDataErrorContexts context)
        {
            return EditingControlFormattedValue;
        }

        // Implements the  
        // IDataGridViewEditingControl.ApplyCellStyleToEditingControl method. 
        public void ApplyCellStyleToEditingControl(DataGridViewCellStyle dataGridViewCellStyle)
        {
            Font = dataGridViewCellStyle.Font;
            ForeColor = dataGridViewCellStyle.ForeColor;
            BackColor = dataGridViewCellStyle.BackColor;
        }

        // Implements the IDataGridViewEditingControl.EditingControlRowIndex  
        // property. 
        public int EditingControlRowIndex
        {
            get
            {
                return _rowIndex;
            }
            set
            {
                _rowIndex = value;
            }
        }

        // Implements the IDataGridViewEditingControl.EditingControlWantsInputKey  
        // method. 
        public bool EditingControlWantsInputKey(Keys key, bool dataGridViewWantsInputKey)
        {
            // Let the DateTimePicker handle the keys listed. 
            switch (key & Keys.KeyCode)
            {
                case Keys.Left:
                case Keys.Up:
                case Keys.Down:
                case Keys.Right:
                case Keys.Home:
                case Keys.End:
                case Keys.PageDown:
                case Keys.PageUp:
                    return true;
                default:
                    return !dataGridViewWantsInputKey;
            }
        }

        // Implements the IDataGridViewEditingControl.PrepareEditingControlForEdit  
        // method. 
        public void PrepareEditingControlForEdit(bool selectAll)
        {
            // No preparation needs to be done.
        }

        // Implements the IDataGridViewEditingControl 
        // .RepositionEditingControlOnValueChange property. 
        public bool RepositionEditingControlOnValueChange
        {
            get { return false; }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlDataGridView property. 
        public DataGridView EditingControlDataGridView
        {
            get { return _dataGridView; }
            set { _dataGridView = value; }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlValueChanged property. 
        public bool EditingControlValueChanged
        {
            get
            {
                return _valueChanged;
            }
            set
            {
                _valueChanged = value;
            }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingPanelCursor property. 
        public Cursor EditingPanelCursor
        {
            get
            {
                return base.Cursor;
            }
        }

        protected void OnValueChanged(EventArgs eventargs)
        {
            // Notify the DataGridView that the contents of the cell 
            // have changed.
            _valueChanged = true;
            EditingControlDataGridView.NotifyCurrentCellDirty(true);
            // base.OnValueChanged(eventargs);
        }

        private DataGridView _dataGridView;
        private bool _valueChanged = false;
        private int _rowIndex;
    }

    internal class VariableNameCell : DataGridViewTextBoxCell
    {
        public IEnumerable<string> Suggestions;

        public override void InitializeEditingControl(
            int rowIndex,
            object initialFormattedValue, DataGridViewCellStyle dataGridViewCellStyle)
        {
                // Set the value of the editing control to the current cell value. 
            base.InitializeEditingControl(rowIndex, initialFormattedValue, dataGridViewCellStyle);
            var ctl = DataGridView.EditingControl as VariableNameEditControl;
            if (ctl != null)
            {
                ctl.Items.Clear();
                ctl.Items.AddRange(Suggestions.ToArray());
                ctl.Value = (Value == null) ? string.Empty : Value.ToString();
            }
        }

        public override Type EditType { get { return typeof(VariableNameEditControl); } }
        public override Type ValueType { get { return typeof(string); } }
        public override object DefaultNewRowValue { get { return string.Empty; } }

        protected override void Paint(
            Graphics graphics,
            Rectangle clipBounds,
            Rectangle cellBounds,
            int rowIndex,
            DataGridViewElementStates cellState,
            object value,
            object formattedValue,
            string errorText,
            DataGridViewCellStyle cellStyle,
            DataGridViewAdvancedBorderStyle advancedBorderStyle,
            DataGridViewPaintParts paintParts)
        {
            base.Paint(graphics, clipBounds, cellBounds, rowIndex, cellState,
                value, formattedValue, errorText, cellStyle,
                advancedBorderStyle, paintParts);
        }

        public override object Clone()
        {
            var c = base.Clone();
            var a = c as VariableNameCell;
            if (a!=null) a.Suggestions = Suggestions;
            return c;
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    internal class RestrictionSettings
    {
        public enum AxisType { AutoAxis, XAxis, YAxis, Constant };
        public AxisType Axis;
        public string LeftValue;
        public string RightValue;

        public string String
        {
            get {
                if (Axis == AxisType.Constant) return LeftValue;
                return Axis.ToString() + ":" + LeftValue + ":" + RightValue;
            }
            set
            {
                var regex = new System.Text.RegularExpressions.Regex("([^:]*):([^:]*):([^:]*)");
                var m = regex.Match(value);
                if (m.Success)
                {
                    AxisType a;
                    if (Enum.TryParse(m.Groups[1].Value, out a))
                    {
                        Axis = a;
                        LeftValue = m.Groups[2].Value;
                        RightValue = m.Groups[3].Value;
                        return;
                    }
                }
                Axis = AxisType.Constant;
                RightValue = String.Empty;
                LeftValue = value;
            }
        }
    }

    internal class VariableRestrictionEditControl : UserControl, IDataGridViewEditingControl
    {
        public VariableRestrictionEditControl()
        {
            this.TextChanged += (sender, e) => { OnValueChanged(e); };
        }

        // Implements the IDataGridViewEditingControl.EditingControlFormattedValue  
        // property. 
        public object EditingControlFormattedValue
        {
            get { return Value.String; }
            set { Value.String = value.ToString(); }
        }

        internal readonly RestrictionSettings Value = new RestrictionSettings();

        // Implements the  
        // IDataGridViewEditingControl.GetEditingControlFormattedValue method. 
        public object GetEditingControlFormattedValue(DataGridViewDataErrorContexts context)
        {
            return EditingControlFormattedValue;
        }

        // Implements the  
        // IDataGridViewEditingControl.ApplyCellStyleToEditingControl method. 
        public void ApplyCellStyleToEditingControl(DataGridViewCellStyle dataGridViewCellStyle)
        {
            Font = dataGridViewCellStyle.Font;
            ForeColor = dataGridViewCellStyle.ForeColor;
            BackColor = dataGridViewCellStyle.BackColor;
        }

        // Implements the IDataGridViewEditingControl.EditingControlRowIndex  
        // property. 
        public int EditingControlRowIndex
        {
            get { return _rowIndex; }
            set { _rowIndex = value; }
        }

        // Implements the IDataGridViewEditingControl.EditingControlWantsInputKey  
        // method. 
        public bool EditingControlWantsInputKey(Keys key, bool dataGridViewWantsInputKey)
        {
            // Let the DateTimePicker handle the keys listed. 
            switch (key & Keys.KeyCode)
            {
                case Keys.Left:
                case Keys.Up:
                case Keys.Down:
                case Keys.Right:
                case Keys.Home:
                case Keys.End:
                case Keys.PageDown:
                case Keys.PageUp:
                    return true;
                default:
                    return !dataGridViewWantsInputKey;
            }
        }

        // Implements the IDataGridViewEditingControl.PrepareEditingControlForEdit  
        // method. 
        public void PrepareEditingControlForEdit(bool selectAll)
        {
            BuildChildControls();
        }

        private void BuildChildControls()
        {
            SuspendLayout();
            Controls.Clear();

            var type = new ComboBox();
            type.Location = new Point(0, 0);
            type.Width = Width / 3;
            type.Height = Height;
            type.Items.AddRange(Enum.GetNames(typeof(RestrictionSettings.AxisType)));
            type.DropDownStyle = ComboBoxStyle.DropDownList;
            type.SelectedIndex = (int)Value.Axis;
            type.SelectedIndexChanged +=
                (object sender, EventArgs e) 
                    => { Value.Axis = (RestrictionSettings.AxisType)(((ComboBox)sender).SelectedIndex); OnValueChanged(e); BuildChildControls(); };
            Controls.Add(type);

            if (Value.Axis == RestrictionSettings.AxisType.Constant)
            {
                var consts = new TextBox();
                consts.Location = new Point(Width / 3, 0);
                consts.Width = Width * 2 / 3;
                consts.Height = Height;
                consts.Text = Value.LeftValue;
                consts.TextChanged +=
                    (object sender, EventArgs e)
                        => { Value.LeftValue = ((TextBox)sender).Text; OnValueChanged(e); };
                Controls.Add(consts);
            }
            else
            {
                var left = new TextBox();
                left.Location = new Point(Width / 3, 0);
                left.Width = Width / 3;
                left.Height = Height;
                left.Text = Value.LeftValue;
                left.TextChanged +=
                    (object sender, EventArgs e)
                        => { Value.LeftValue = ((TextBox)sender).Text; OnValueChanged(e); };
                var right = new TextBox();
                right.Location = new Point(Width * 2 / 3, 0);
                right.Width = Width / 3;
                right.Height = Height;
                right.Text = Value.RightValue;
                right.TextChanged +=
                    (object sender, EventArgs e)
                        => { Value.RightValue = ((TextBox)sender).Text; OnValueChanged(e); };
                Controls.Add(left);
                Controls.Add(right);
            }

            ResumeLayout();
        }

        // Implements the IDataGridViewEditingControl 
        // .RepositionEditingControlOnValueChange property. 
        public bool RepositionEditingControlOnValueChange
        {
            get { return false; }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlDataGridView property. 
        public DataGridView EditingControlDataGridView
        {
            get { return _dataGridView; }
            set { _dataGridView = value; }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingControlValueChanged property. 
        public bool EditingControlValueChanged
        {
            get { return _valueChanged; }
            set { _valueChanged = value; }
        }

        // Implements the IDataGridViewEditingControl 
        // .EditingPanelCursor property. 
        public Cursor EditingPanelCursor
        {
            get { return base.Cursor; }
        }

        protected void OnValueChanged(EventArgs eventargs)
        {
            // Notify the DataGridView that the contents of the cell 
            // have changed.
            _valueChanged = true;
            EditingControlDataGridView.NotifyCurrentCellDirty(true);
            // base.OnValueChanged(eventargs);
        }

        private DataGridView _dataGridView;
        private bool _valueChanged = false;
        private int _rowIndex;
    }

    internal class VariableRestrictionCell : DataGridViewTextBoxCell
    {
        public override void InitializeEditingControl(
            int rowIndex,
            object initialFormattedValue, DataGridViewCellStyle dataGridViewCellStyle)
        {
            // Set the value of the editing control to the current cell value. 
            base.InitializeEditingControl(rowIndex, initialFormattedValue, dataGridViewCellStyle);
            var ctl = DataGridView.EditingControl as VariableRestrictionEditControl;
            if (ctl != null)
            {
                ctl.Value.String = (Value == null) ? string.Empty : Value.ToString();
            }
        }

        public override Type EditType { get { return typeof(VariableRestrictionEditControl); } }
        public override Type ValueType { get { return typeof(string); } }
        public override object DefaultNewRowValue { get { return string.Empty; } }

        protected override void Paint(
            Graphics graphics,
            Rectangle clipBounds,
            Rectangle cellBounds,
            int rowIndex,
            DataGridViewElementStates cellState,
            object value,
            object formattedValue,
            string errorText,
            DataGridViewCellStyle cellStyle,
            DataGridViewAdvancedBorderStyle advancedBorderStyle,
            DataGridViewPaintParts paintParts)
        {
            base.Paint(graphics, clipBounds, cellBounds, rowIndex, cellState,
                value, formattedValue, errorText, cellStyle,
                advancedBorderStyle, paintParts);
        }
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

    internal class StringPair : INotifyPropertyChanged
    {
        public string Key { get { return _key; } set { _key = value; NotifyPropertyChanged("Key"); } }
        public string Value { get { return _value; } set { _value = value; NotifyPropertyChanged("Value"); } }
        public event PropertyChangedEventHandler PropertyChanged;

        protected void NotifyPropertyChanged(/*[CallerMemberName]*/ string propertyName)
        {
            if (PropertyChanged != null)
                PropertyChanged.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private string _key;
        private string _value;
    };

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
                        CellTemplate = new VariableNameCell { Suggestions = sugg },
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
                        CellTemplate = new VariableRestrictionCell(),
                        Name = "Value",
                        HeaderText = "Restrictions",
                        ToolTipText = "Restrictions applied to the variable",
                        DataPropertyName = "Value",
                        FillWeight = 70,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });
            }

            _variablesList = new BindingList<StringPair>();
            foreach (var i in _context.GraphContext.Variables)
                _variablesList.Add(new StringPair { Key = i.Key, Value = i.Value });
            _variables.DataSource = _variablesList;

            _okButton.Click += _okButton_Click;
        }

        private void _okButton_Click(object sender, EventArgs e)
        {
            // commit the variables to the document
            _context.GraphContext.Variables.Clear();
            foreach (var i in _variablesList)
                _context.GraphContext.Variables.Add(i.Key ?? string.Empty, i.Value ?? string.Empty);

            _context.Invalidate();
            DialogResult = DialogResult.OK;
        }

        private NodeEditorCore.IDiagramDocument _context;
        private BindingList<StringPair> _variablesList;
    }
}
