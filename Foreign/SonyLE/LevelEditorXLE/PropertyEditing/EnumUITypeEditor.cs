// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms.Design;
using System.Drawing.Design;
using System.ComponentModel;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Controls.PropertyEditing;

namespace LevelEditorXLE.PropertyEditing
{
    class EnumUITypeEditor : UITypeEditor, IAnnotatedParams, IPropertyEditor
    {
        #region UITypeEditor Members

        private IWindowsFormsEditorService _editorService;

        public override UITypeEditorEditStyle GetEditStyle(ITypeDescriptorContext context)
        {
            return UITypeEditorEditStyle.DropDown;
        }

        public override object EditValue(ITypeDescriptorContext context, IServiceProvider provider, object value)
        {
            _editorService = (IWindowsFormsEditorService)provider.GetService(typeof(IWindowsFormsEditorService));

            var lb = new ListBox();
            Sce.Atf.Applications.SkinService.ApplyActiveSkin(lb);
            lb.SelectionMode = SelectionMode.One;
            lb.SelectedValueChanged += OnListBoxSelectedValueChanged;

            lb.DisplayMember = "Name";
            lb.ValueMember = "Value";

            for (int i = 0; i < m_names.Length; i++)
            {
                lb.Items.Add(new { Name = m_names[i], Value = m_values[i] });
                if (m_names[i].Equals(value) || m_values[i].Equals(value))
                    lb.SelectedIndex = i;
            }

            _editorService.DropDownControl(lb);
            if (lb.SelectedItem == null) // no selection, return the passed-in value as is
                return value;

            int index = lb.SelectedIndex;
            if (index >= 0)
            {
                object newValue = null;
                if (value is int)
                    newValue = m_values[index];
                else
                    newValue = m_names[index];
                // be careful to return the same object if the value didn't change
                if (!newValue.Equals(value))
                    value = newValue;
            }
            return value;
        }

        private void OnListBoxSelectedValueChanged(object sender, EventArgs e)
        {
            // close the drop down as soon as something is clicked
            _editorService.CloseDropDown();
        }

        #endregion

        #region IPropertyEditor Members
        public Control GetEditingControl(PropertyEditorControlContext context)
        {
            var combo = new ComboBox();
            Sce.Atf.Applications.SkinService.ApplyActiveSkin(combo);
            combo.DropDownStyle     = ComboBoxStyle.DropDownList;
            combo.FlatStyle         = FlatStyle.Flat;
            combo.DisplayMember     = "Name";
            combo.ValueMember       = "Value";

            var value = context.GetValue();
            for (int i = 0; i < m_names.Length; i++)
            {
                combo.Items.Add(new { Name = m_names[i], Value = m_values[i] });
                if (m_names[i].Equals(value) || m_values[i].Equals(value))
                    combo.SelectedIndex = i;
            }

            bool useIndex = value is int;
            combo.SelectedIndexChanged += 
                (object sender, EventArgs args) =>
                {
                    dynamic o = combo.SelectedItem;
                    context.SetValue(useIndex ? o.Value : o.Name);
                };

            return combo;
        }
        #endregion

        public void DefineEnum(string[] names)
        {
            EnumUtil.ParseEnumDefinitions(names, out m_names, out m_values);
        }

        #region IAnnotatedParams Members

        public void Initialize(string[] parameters)
        {
            DefineEnum(parameters);
        }

        #endregion

        private string[] m_names = EmptyArray<string>.Instance;
        private int[] m_values = EmptyArray<int>.Instance;
    }
}
