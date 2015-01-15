using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Collections.ObjectModel;
using System.Windows.Input;
using System.ComponentModel;

namespace NodeEditorRibbon.ViewModel
{
    public class MenuButtonData : ControlData
    {
        public MenuButtonData()
            : this(false)
        {
        }

        public MenuButtonData(bool isApplicationMenu)
        {
            _isApplicationMenu = isApplicationMenu;
        }

        public bool IsVerticallyResizable
        {
            get
            {
                return _isVerticallyResizable;
            }

            set
            {
                if (_isVerticallyResizable != value)
                {
                    _isVerticallyResizable = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("IsVerticallyResizable"));
                }
            }
        }

        public bool IsHorizontallyResizable
        {
            get
            {
                return _isHorizontallyResizable;
            }

            set
            {
                if (_isHorizontallyResizable != value)
                {
                    _isHorizontallyResizable = value;
                    OnPropertyChanged(new PropertyChangedEventArgs("IsHorizontallyResizable"));
                }
            }
        }

        private bool _isVerticallyResizable, _isHorizontallyResizable;

        [DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
        public ObservableCollection<ControlData> ControlDataCollection
        {
            get
            {
                if (_controlDataCollection == null)
                {
                    _controlDataCollection = new ObservableCollection<ControlData>();

                    if (_nestingDepth <= ViewModelData.MenuItemNestingCount)
                    {
                        _nestingDepth++;

                        Uri smallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative);
                        Uri largeImage = new Uri("/NodeEditorRibbon;component/Images/Paste_32x32.png", UriKind.Relative);

                        for (int i = 0; i < ViewModelData.GalleryCount; i++)
                        {
                            GalleryData galleryData = new GalleryData()
                            {
                                Label = "Gallery " + i,
                                SmallImage = smallImage,
                                LargeImage = largeImage,
                                ToolTipTitle = "ToolTip Title",
                                ToolTipDescription = "ToolTip Description",
                                ToolTipImage = smallImage,
                                Command = ViewModelData.DefaultCommand
                            };
                            galleryData.SelectedItem = galleryData.CategoryDataCollection[0].GalleryItemDataCollection[ViewModelData.GalleryItemCount - 1];

                            _controlDataCollection.Add(galleryData);
                        }
                        for (int i = 0; i < ViewModelData.MenuItemCount; i++)
                        {
                            MenuItemData menuItemData = _isApplicationMenu ?
                                new ApplicationMenuItemData(true) :
                                new MenuItemData(false);
                            menuItemData.Label = "MenuItem " + i;
                            menuItemData.SmallImage = smallImage;
                            menuItemData.LargeImage = largeImage;
                            menuItemData.ToolTipTitle = "ToolTip Title";
                            menuItemData.ToolTipDescription = "ToolTip Description";
                            menuItemData.ToolTipImage = smallImage;
                            menuItemData.Command = ViewModelData.DefaultCommand;
                            menuItemData._nestingDepth = this._nestingDepth;

                            _controlDataCollection.Add(menuItemData);
                        }

                        _controlDataCollection.Add(new SeparatorData());

                        for (int i = 0; i < ViewModelData.SplitMenuItemCount; i++)
                        {
                            SplitMenuItemData splitMenuItemData = _isApplicationMenu ?
                                new ApplicationSplitMenuItemData(true) :
                                new SplitMenuItemData(false);
                            splitMenuItemData.Label = "SplitMenuItem " + i;
                            splitMenuItemData.SmallImage = smallImage;
                            splitMenuItemData.LargeImage = largeImage;
                            splitMenuItemData.ToolTipTitle = "ToolTip Title";
                            splitMenuItemData.ToolTipDescription = "ToolTip Description";
                            splitMenuItemData.ToolTipImage = smallImage;
                            splitMenuItemData.Command = ViewModelData.DefaultCommand;
                            splitMenuItemData._nestingDepth = this._nestingDepth;
                            splitMenuItemData.IsCheckable = true;

                            _controlDataCollection.Add(splitMenuItemData);
                        }
                    }
                }
                return _controlDataCollection;
            }
        }
        private ObservableCollection<ControlData> _controlDataCollection;
        private int _nestingDepth;
        private bool _isApplicationMenu;
    }
}
