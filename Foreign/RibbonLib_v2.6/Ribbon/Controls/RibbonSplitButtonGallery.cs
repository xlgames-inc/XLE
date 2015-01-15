//*****************************************************************************
//
//  File:       RibbonSplitButtonGallery.cs
//
//  Contents:   Helper class that wraps a ribbon split button gallery control.
//
//*****************************************************************************

using RibbonLib.Controls.Events;
using RibbonLib.Controls.Properties;
using RibbonLib.Interop;
using System;

namespace RibbonLib.Controls
{
    public class RibbonSplitButtonGallery : BaseRibbonControl,
        IBooleanValuePropertyProvider,
        IGalleryPropertiesProvider,
        IEnabledPropertiesProvider, 
        IKeytipPropertiesProvider,
        ILabelPropertiesProvider,
        IImagePropertiesProvider,
        ITooltipPropertiesProvider,
        IExecuteEventsProvider,
        IPreviewEventsProvider
    {
        private BooleanValuePropertyProvider _booleanValuePropertyProvider;
        private GalleryPropertiesProvider _galleryPropertiesProvider;
        private EnabledPropertiesProvider _enabledPropertiesProvider;
        private KeytipPropertiesProvider _keytipPropertiesProvider;
        private LabelPropertiesProvider _labelPropertiesProvider;
        private ImagePropertiesProvider _imagePropertiesProvider;
        private TooltipPropertiesProvider _tooltipPropertiesProvider;
        private ExecuteEventsProvider _executeEventsProvider;
        private PreviewEventsProvider _previewEventsProvider;

        public RibbonSplitButtonGallery(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            AddPropertiesProvider(_booleanValuePropertyProvider = new BooleanValuePropertyProvider(ribbon, commandId));
            AddPropertiesProvider(_galleryPropertiesProvider = new GalleryPropertiesProvider(ribbon, commandId, this));
            AddPropertiesProvider(_enabledPropertiesProvider = new EnabledPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_keytipPropertiesProvider = new KeytipPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_labelPropertiesProvider = new LabelPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_imagePropertiesProvider = new ImagePropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_tooltipPropertiesProvider = new TooltipPropertiesProvider(ribbon, commandId));

            AddEventsProvider(_executeEventsProvider = new ExecuteEventsProvider(this));
            AddEventsProvider(_previewEventsProvider = new PreviewEventsProvider(this));
        }

        #region IBooleanValuePropertyProvider Members

        public bool BooleanValue
        {
            get
            {
                return _booleanValuePropertyProvider.BooleanValue;
            }
            set
            {
                _booleanValuePropertyProvider.BooleanValue = value;
            }
        }

        #endregion

        #region IGalleryPropertiesProvider Members

        public IUICollection Categories
        {
            get
            {
                return _galleryPropertiesProvider.Categories;
            }
        }

        public IUICollection ItemsSource
        {
            get
            {
                return _galleryPropertiesProvider.ItemsSource;
            }
        }

        public uint SelectedItem
        {
            get
            {
                return _galleryPropertiesProvider.SelectedItem;
            }
            set
            {
                _galleryPropertiesProvider.SelectedItem = value;
            }
        }

        public event EventHandler<EventArgs> CategoriesReady
        {
            add
            {
                _galleryPropertiesProvider.CategoriesReady += value;
            }
            remove
            {
                _galleryPropertiesProvider.CategoriesReady -= value;
            }
        }

        public event EventHandler<EventArgs> ItemsSourceReady
        {
            add
            {
                _galleryPropertiesProvider.ItemsSourceReady += value;
            }
            remove
            {
                _galleryPropertiesProvider.ItemsSourceReady -= value;
            }
        }

        #endregion

        #region IEnabledPropertiesProvider Members

        public bool Enabled
        {
            get
            {
                return _enabledPropertiesProvider.Enabled;
            }
            set
            {
                _enabledPropertiesProvider.Enabled = value;
            }
        }

        #endregion

        #region IKeytipPropertiesProvider Members

        public string Keytip
        {
            get
            {
                return _keytipPropertiesProvider.Keytip;
            }
            set
            {
                _keytipPropertiesProvider.Keytip = value;
            }
        }

        #endregion

        #region ILabelPropertiesProvider Members

        public string Label
        {
            get
            {
                return _labelPropertiesProvider.Label;
            }
            set
            {
                _labelPropertiesProvider.Label = value;
            }
        }

        #endregion

        #region IImagePropertiesProvider Members

        public IUIImage LargeImage
        {
            get
            {
                return _imagePropertiesProvider.LargeImage;
            }
            set
            {
                _imagePropertiesProvider.LargeImage = value;
            }
        }

        public IUIImage SmallImage
        {
            get
            {
                return _imagePropertiesProvider.SmallImage;
            }
            set
            {
                _imagePropertiesProvider.SmallImage = value;
            }
        }

        public IUIImage LargeHighContrastImage
        {
            get
            {
                return _imagePropertiesProvider.LargeHighContrastImage;
            }
            set
            {
                _imagePropertiesProvider.LargeHighContrastImage = value;
            }
        }

        public IUIImage SmallHighContrastImage
        {
            get
            {
                return _imagePropertiesProvider.SmallHighContrastImage;
            }
            set
            {
                _imagePropertiesProvider.SmallHighContrastImage = value;
            }
        }

        #endregion

        #region ITooltipPropertiesProvider Members

        public string TooltipTitle
        {
            get
            {
                return _tooltipPropertiesProvider.TooltipTitle;
            }
            set
            {
                _tooltipPropertiesProvider.TooltipTitle = value;
            }
        }

        public string TooltipDescription
        {
            get
            {
                return _tooltipPropertiesProvider.TooltipDescription;
            }
            set
            {
                _tooltipPropertiesProvider.TooltipDescription = value;
            }
        }

        #endregion

        #region IExecuteEventsProvider Members

        public event EventHandler<ExecuteEventArgs> ExecuteEvent
        {
            add
            {
                _executeEventsProvider.ExecuteEvent += value;
            }
            remove
            {
                _executeEventsProvider.ExecuteEvent -= value;
            }
        }

        #endregion

        #region IPreviewEventsProvider Members

        public event EventHandler<ExecuteEventArgs> PreviewEvent
        {
            add
            {
                _previewEventsProvider.PreviewEvent += value;
            }
            remove
            {
                _previewEventsProvider.PreviewEvent -= value;
            }
        }

        public event EventHandler<ExecuteEventArgs> CancelPreviewEvent
        {
            add
            {
                _previewEventsProvider.CancelPreviewEvent += value;
            }
            remove
            {
                _previewEventsProvider.CancelPreviewEvent -= value;
            }
        }

        #endregion
    }
}
