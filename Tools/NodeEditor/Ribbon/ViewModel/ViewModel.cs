using System;
using System.Windows.Input;
using RibbonSamples.Common;

namespace NodeEditorRibbon.ViewModel
{
    public static class ViewModelData
    {
        internal const int TabCount = 4;
        internal const int ContextualTabGroupCount = 2;
        internal const int GroupCount = 3;
        internal const int ControlCount = 5;
        internal const int ButtonCount = 1;
        internal const int ToggleButtonCount = 1;
        internal const int RadioButtonCount = 1;
        internal const int CheckBoxCount = 1;
        internal const int TextBoxCount = 1;
        internal const int MenuButtonCount = 1;
        internal const int MenuItemCount = 2;
        internal const int SplitButtonCount = 1;
        internal const int SplitMenuItemCount = 2;
        internal const int GalleryCount = 1;
        internal const int GalleryCategoryCount = 3;
        internal const int GalleryItemCount = 10;
        internal const int MenuItemNestingCount = 2;
        internal const int ComboBoxCount = 1;

        public static RibbonData RibbonData
        {
            get
            {
                if (_data == null)
                {
                    _data = new RibbonData();
                }
                return _data;
            }
        }

        public static ICommand DefaultCommand
        {
            get
            {
                if (_defaultCommand == null)
                {
                    _defaultCommand = new DelegateCommand(DefaultExecuted, DefaultCanExecute);
                }
                return _defaultCommand;
            }
        }

        private static void DefaultExecuted()
        {
        }

        private static bool DefaultCanExecute()
        {
            return true;
        }

        [ThreadStatic]
        private static RibbonData _data;
        private static ICommand _defaultCommand;
    }
}
