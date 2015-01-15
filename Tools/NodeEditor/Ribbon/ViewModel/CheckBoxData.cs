using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace NodeEditorRibbon.ViewModel
{
    public class CheckBoxData : ControlData
    {
        public bool IsChecked
        {
            get
            {
                return _isChecked;
            }

            set
            {
                if (_isChecked != value)
                {
                    _isChecked = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("IsChecked"));
                }
            }
        }
        private bool _isChecked;
    }
}
