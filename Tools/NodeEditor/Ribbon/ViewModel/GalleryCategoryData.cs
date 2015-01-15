using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Collections.ObjectModel;
using System.Windows.Input;
using System.ComponentModel;

namespace NodeEditorRibbon.ViewModel
{
    public class GalleryCategoryData : ControlData
    {
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
        public ObservableCollection<GalleryItemData> GalleryItemDataCollection
        {
            get
            {
                if (_controlDataCollection == null)
                {
                    _controlDataCollection = new ObservableCollection<GalleryItemData>();

                    Uri smallImage = new Uri("/NodeEditorRibbon;component/Images/Paste_16x16.png", UriKind.Relative);
                    Uri largeImage = new Uri("/NodeEditorRibbon;component/Images/Paste_32x32.png", UriKind.Relative);

                    for (int i = 0; i < ViewModelData.GalleryItemCount; i++)
                    {
                        _controlDataCollection.Add(new GalleryItemData()
                        {
                            Label = "GalleryItem " + i,
                            SmallImage = smallImage,
                            LargeImage = largeImage,
                            ToolTipTitle = "ToolTip Title",
                            ToolTipDescription = "ToolTip Description",
                            ToolTipImage = smallImage,
                            Command = ViewModelData.DefaultCommand
                        });
                    }
                }
                return _controlDataCollection;
            }
        }
        private ObservableCollection<GalleryItemData> _controlDataCollection;
    }

    public class GalleryCategoryData<T> : ControlData
    {
        [DesignerSerializationVisibility(DesignerSerializationVisibility.Content)]
        public ObservableCollection<T> GalleryItemDataCollection
        {
            get
            {
                if (_controlDataCollection == null)
                {
                    _controlDataCollection = new ObservableCollection<T>();
                }
                return _controlDataCollection;
            }
        }
        private ObservableCollection<T> _controlDataCollection;
    }
}
