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

namespace MaterialTool
{
    internal class VariableNameEditControl : ComboBox, IDataGridViewEditingControl
    {
        public VariableNameEditControl()
        {
            this.TextChanged += (sender, e) => { OnValueChanged(e); };
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
                ctl.DataSource = Suggestions;
                ctl.Value = (Value == null) ? string.Empty : Value.ToString();
            }
        }

        public override Type EditType { get { return typeof(VariableNameEditControl); } }

        public override Type ValueType
        {
            get { return typeof(string); }
        }
        
        public override object DefaultNewRowValue
        {
            get { return string.Empty; }
        }

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

    internal class StringPair : INotifyPropertyChanged
    {
        public string Key { get { return _key; } set { _key = value; NotifyPropertyChanged("Key"); } }
        public string Value { get { return _value; } set { _value = value; NotifyPropertyChanged("Value"); } }
        public event PropertyChangedEventHandler PropertyChanged;

        public StringPair() { _propertyChangedContext = System.Threading.SynchronizationContext.Current; }

        protected void NotifyPropertyChanged(/*[CallerMemberName]*/ string propertyName)
        {
            if (PropertyChanged != null)
                PropertyChanged.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private System.Threading.SynchronizationContext _propertyChangedContext;
        private string _key;
        private string _value;
    };

    public partial class DiagramSettings : Form
    {
        public DiagramSettings(ShaderPatcherLayer.Document doc)
        {
            InitializeComponent();
            _doc = doc;

            var sugg = new List<string> { "in0", "in1", "out0", "out1" };

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
                        FillWeight = 70,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });

                _variables.Columns.Add(
                    new DataGridViewTextBoxColumn()
                    {
                        Name = "Value",
                        HeaderText = "Restrictions",
                        ToolTipText = "Restrictions applied to the variable",
                        DataPropertyName = "Value",
                        FillWeight = 30,
                        AutoSizeMode = DataGridViewAutoSizeColumnMode.Fill
                    });
            }

            var bindingList = new BindingList<StringPair>();
            bindingList.Add(new StringPair { Key = "One", Value = "1" });
            bindingList.Add(new StringPair { Key = "Two", Value = "2" });
            _variables.DataSource = bindingList;
        }

        private ShaderPatcherLayer.Document _doc;
    }
}
