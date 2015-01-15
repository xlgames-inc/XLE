using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ComponentModel;
using System.Windows.Input;

namespace NodeEditorRibbon.ViewModel
{
    public class ControlData : INotifyPropertyChanged
    {
        public string Label
        {
            get
            {
                return _label;
            }

            set
            {
                if (_label != value)
                {
                    _label = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("Label"));
                }
            }
        }
        private string _label;

        public Uri LargeImage
        {
            get
            {
                return _largeImage;
            }

            set
            {
                if (_largeImage != value)
                {
                    _largeImage = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("LargeImage"));
                }
            }
        }
        private Uri _largeImage;

        public Uri SmallImage
        {
            get
            {
                return _smallImage;
            }

            set
            {
                if (_smallImage != value)
                {
                    _smallImage = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("SmallImage"));
                }
            }
        }
        private Uri _smallImage;

        public string ToolTipTitle
        {
            get
            {
                return _toolTipTitle;
            }

            set
            {
                if (_toolTipTitle != value)
                {
                    _toolTipTitle = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipTitle"));
                }
            }
        }
        private string _toolTipTitle;

        public string ToolTipDescription
        {
            get
            {
                return _toolTipDescription;
            }

            set
            {
                if (_toolTipDescription != value)
                {
                    _toolTipDescription = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipDescription"));
                }
            }
        }
        private string _toolTipDescription;

        public Uri ToolTipImage
        {
            get
            {
                return _toolTipImage;
            }

            set
            {
                if (_toolTipImage != value)
                {
                    _toolTipImage = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipImage"));
                }
            }
        }
        private Uri _toolTipImage;

        public string ToolTipFooterTitle
        {
            get
            {
                return _toolTipFooterTitle;
            }

            set
            {
                if (_toolTipFooterTitle != value)
                {
                    _toolTipFooterTitle = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipFooterTitle"));
                }
            }
        }
        private string _toolTipFooterTitle;

        public string ToolTipFooterDescription
        {
            get
            {
                return _toolTipFooterDescription;
            }

            set
            {
                if (_toolTipFooterDescription != value)
                {
                    _toolTipFooterDescription = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipFooterDescription"));
                }
            }
        }
        private string _toolTipFooterDescription;

        public Uri ToolTipFooterImage
        {
            get
            {
                return _toolTipFooterImage;
            }

            set
            {
                if (_toolTipFooterImage != value)
                {
                    _toolTipFooterImage = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("ToolTipFooterImage"));
                }
            }
        }
        private Uri _toolTipFooterImage;

        public ICommand Command
        {
            get
            {
                return _command;
            }

            set
            {
                if (_command != value)
                {
                    _command = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("Command"));
                }
            }
        }
        private ICommand _command;

        public string KeyTip
        {
            get
            {
                return _keyTip;
            }

            set
            {
                if (_keyTip != value)
                {
                    _keyTip = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("KeyTip"));
                }
            }
        }
        private string _keyTip;

        #region INotifyPropertyChanged Members

        public event PropertyChangedEventHandler PropertyChanged;

        protected void OnPropertyChanged(PropertyChangedEventArgs e)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, e);
            }
        }

        #endregion
    }

}
