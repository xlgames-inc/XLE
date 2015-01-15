using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;

namespace NodeEditorRibbon.ViewModel
{
    public class SplitButtonData : MenuButtonData
    {
        public SplitButtonData()
            : this(false)
        {
        }

        public SplitButtonData(bool isApplicationMenu)
            : base(isApplicationMenu)
        {
        }

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

        public bool IsCheckable
        {
            get
            {
                return _isCheckable;
            }

            set
            {
                if (_isCheckable != value)
                {
                    _isCheckable = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("IsCheckable"));
                }
            }
        }
        private bool _isCheckable;

        public ButtonData DropDownButtonData
        {
            get
            {
                if (_dropDownButtonData == null)
                {
                    _dropDownButtonData = new ButtonData();
                }

                return _dropDownButtonData;
            }
        }
        private ButtonData _dropDownButtonData;
    }
}
