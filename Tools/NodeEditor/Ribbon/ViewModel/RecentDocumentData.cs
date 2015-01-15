using System.ComponentModel;

namespace NodeEditorRibbon.ViewModel
{
    public class RecentDocumentData : ToggleButtonData
    {
        public int Index
        {
            get
            {
                return _index;
            }

            set
            {
                if (_index != value)
                {
                    _index = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("Index"));
                }
            }
        }
        private int _index;
    }
}
