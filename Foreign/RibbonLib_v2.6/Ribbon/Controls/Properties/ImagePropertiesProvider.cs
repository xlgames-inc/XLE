//*****************************************************************************
//
//  File:       ImagePropertiesProvider.cs
//
//  Contents:   Definition for image properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for image properties provider interface
    /// </summary>
    public interface IImagePropertiesProvider
    {
        /// <summary>
        /// Large image property
        /// </summary>
        IUIImage LargeImage { get; set; }

        /// <summary>
        /// Small image property
        /// </summary>
        IUIImage SmallImage { get; set; }

        /// <summary>
        /// Large high contrast image property
        /// </summary>
        IUIImage LargeHighContrastImage { get; set; }

        /// <summary>
        /// Small high contrast image property
        /// </summary>
        IUIImage SmallHighContrastImage { get; set; }
    }

    /// <summary>
    /// Implementation of IImagePropertiesProvider
    /// </summary>
    public class ImagePropertiesProvider : BasePropertiesProvider, IImagePropertiesProvider
    {
        /// <summary>
        /// ImagePropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public ImagePropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.LargeImage);
            _supportedProperties.Add(RibbonProperties.SmallImage);
            _supportedProperties.Add(RibbonProperties.LargeHighContrastImage);
            _supportedProperties.Add(RibbonProperties.SmallHighContrastImage);
        }
        
        private IUIImage _largeImage;
        private IUIImage _smallImage;
        private IUIImage _largeHighContrastImage;
        private IUIImage _smallHighContrastImage;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.LargeImage)
            {
                if (_largeImage != null)
                {
                    newValue.SetIUnknown(_largeImage);
                }
            }
            else if (key == RibbonProperties.SmallImage)
            {
                if (_smallImage != null)
                {
                    newValue.SetIUnknown(_smallImage);
                }
            }
            else if (key == RibbonProperties.LargeHighContrastImage)
            {
                if (_largeHighContrastImage != null)
                {
                    newValue.SetIUnknown(_largeHighContrastImage);
                }
            }
            else if (key == RibbonProperties.SmallHighContrastImage)
            {
                if (_smallHighContrastImage != null)
                {
                    newValue.SetIUnknown(_smallHighContrastImage);
                }
            }

            return HRESULT.S_OK;
        }

        #region IImagePropertiesProvider Members

        /// <summary>
        /// Large image property
        /// </summary>
        public IUIImage LargeImage
        {
            get
            {
                return _largeImage;
            }
            set
            {
                _largeImage = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.LargeImage));
                }
            }
        }

        /// <summary>
        /// Small image property
        /// </summary>
        public IUIImage SmallImage
        {
            get
            {
                return _smallImage;
            }
            set
            {
                _smallImage = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.SmallImage));
                }
            }
        }

        /// <summary>
        /// Large high contrast image property
        /// </summary>
        public IUIImage LargeHighContrastImage
        {
            get
            {
                return _largeHighContrastImage;
            }
            set
            {
                _largeHighContrastImage = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.LargeHighContrastImage));
                }
            }
        }

        /// <summary>
        /// Small high contrast image property
        /// </summary>
        public IUIImage SmallHighContrastImage
        {
            get
            {
                return _smallHighContrastImage;
            }
            set
            {
                _smallHighContrastImage = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.SmallHighContrastImage));
                }
            }
        }

        #endregion
    }
}
