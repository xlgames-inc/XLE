//*****************************************************************************
//
//  File:       FontControlPropertiesProvider.cs
//
//  Contents:   Definition for font control properties provider 
//
//*****************************************************************************

using System;
using System.Drawing;
using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for font control properties provider interface
    /// </summary>
    public interface IFontControlPropertiesProvider
    {
        /// <summary>
        /// Family property
        /// </summary>
        string Family { get; set; }

        /// <summary>
        /// Size property
        /// </summary>
        decimal Size { get; set; }

        /// <summary>
        /// Bold property
        /// </summary>
        FontProperties Bold { get; set; }

        /// <summary>
        /// Italic property
        /// </summary>
        FontProperties Italic { get; set; }

        /// <summary>
        /// Underline property
        /// </summary>
        FontUnderline Underline { get; set; }

        /// <summary>
        /// Strikethrough property
        /// </summary>
        FontProperties Strikethrough { get; set; }

        /// <summary>
        /// Foreground color property
        /// </summary>
        Color ForegroundColor { get; set; }
        
        /// <summary>
        /// Background color property
        /// </summary>
        Color BackgroundColor { get; set; }

        /// <summary>
        /// Vertical positioning property
        /// </summary>
        FontVerticalPosition VerticalPositioning { get; set; }
    }

    /// <summary>
    /// Implementation of IFontControlPropertiesProvider
    /// </summary>
    public class FontControlPropertiesProvider : BasePropertiesProvider, IFontControlPropertiesProvider
    {
        /// <summary>
        /// FontControlPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public FontControlPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            // add supported properties
            _supportedProperties.Add(RibbonProperties.FontProperties);
        }

        private string _family;
        private decimal? _size;
        private FontProperties? _bold;
        private FontProperties? _italic;
        private FontUnderline? _underline;
        private FontProperties? _strikethrough;
        private Color? _foregroundColor;
        private SwatchColorType? _foregroundColorType;
        private Color? _backgroundColor;
        private SwatchColorType? _backgroundColorType;
        private FontVerticalPosition? _verticalPositioning;
        
        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.FontProperties)
            {
                if (currentValue != null)
                {
                    // get current font properties
                    IPropertyStore fontProperties = (IPropertyStore)currentValue.PropVariant.Value;

                    // set family
                    if ( (_family == null) || (_family.Trim() == string.Empty) )
                    {
                        PropVariant propFamily = PropVariant.Empty;
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Family, ref propFamily);
                    }
                    else
                    {
                        PropVariant propFamily = PropVariant.FromObject(_family);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Family, ref propFamily);
                    }

                    // set size
                    if (_size.HasValue)
                    {
                        PropVariant propSize = PropVariant.FromObject(_size.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Size, ref propSize);
                    }

                    // set bold
                    if (_bold.HasValue)
                    {
                        PropVariant propBold = PropVariant.FromObject((uint)_bold.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Bold, ref propBold);
                    }

                    // set italic
                    if (_italic.HasValue)
                    {
                        PropVariant propItalic = PropVariant.FromObject((uint)_italic.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Italic, ref propItalic);
                    }
                    
                    // set underline
                    if (_underline.HasValue)
                    {
                        PropVariant propUnderline = PropVariant.FromObject((uint)_underline.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Underline, ref propUnderline);
                    }
                    
                    // set strikethrough
                    if (_strikethrough.HasValue)
                    {
                        PropVariant propStrikethrough = PropVariant.FromObject((uint)_strikethrough.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_Strikethrough, ref propStrikethrough);
                    }
                    
                    // set foregroundColor
                    if (_foregroundColor.HasValue)
                    {
                        PropVariant propForegroundColor = PropVariant.FromObject((uint)ColorTranslator.ToWin32(_foregroundColor.Value));
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_ForegroundColor, ref propForegroundColor);
                    }
                    
                    // set foregroundColorType
                    if (_foregroundColorType.HasValue)
                    {
                        PropVariant propForegroundColorType = PropVariant.FromObject((uint)_foregroundColorType.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_ForegroundColorType, ref propForegroundColorType);
                    }
                    
                    // set backgroundColor
                    if (_backgroundColor.HasValue)
                    {
                        PropVariant propBackgroundColor = PropVariant.FromObject((uint)ColorTranslator.ToWin32(_backgroundColor.Value));
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_BackgroundColor, ref propBackgroundColor);
                    }
                    
                    // set backgroundColorType
                    if (_backgroundColorType.HasValue)
                    {
                        PropVariant propBackgroundColorType = PropVariant.FromObject((uint)_backgroundColorType.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_BackgroundColorType, ref propBackgroundColorType);
                    }
                    
                    // set verticalPositioning
                    if (_verticalPositioning.HasValue)
                    {
                        PropVariant propVerticalPositioning = PropVariant.FromObject((uint)_verticalPositioning.Value);
                        fontProperties.SetValue(ref RibbonProperties.FontProperties_VerticalPositioning, ref propVerticalPositioning);
                    }
                    
                    // set new font properties
                    newValue.SetIUnknown(fontProperties);
                }
            }

            return HRESULT.S_OK;
        }

        /// <summary>
        /// Font properties property
        /// </summary>
        private IPropertyStore FontProperties
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant iunknownValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.FontProperties, out iunknownValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (IPropertyStore)iunknownValue.Value;
                    }
                }

                return null;
            }
        }

        #region IFontControlPropertiesProvider Members

        /// <summary>
        /// Family property
        /// </summary>
        public string Family 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propFamily;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Family, out propFamily);
                    return (string)propFamily.Value;
                }

                return _family;
            }
            set
            {
                _family = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Size property
        /// </summary>
        public decimal Size 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propSize;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Size, out propSize);
                    return (decimal)propSize.Value;
                }

                return _size.GetValueOrDefault();
            }
            set
            {
                _size = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Bold property
        /// </summary>
        public FontProperties Bold 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propBold;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Bold, out propBold);
                    return (FontProperties)(uint)propBold.Value;
                }

                return _bold.GetValueOrDefault(RibbonLib.Interop.FontProperties.NotAvailable);
            }
            set
            {
                _bold = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Italic property
        /// </summary>
        public FontProperties Italic 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propItalic;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Italic, out propItalic);
                    return (FontProperties)(uint)propItalic.Value;
                }

                return _italic.GetValueOrDefault(RibbonLib.Interop.FontProperties.NotAvailable);
            }
            set
            {
                _italic = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Underline property
        /// </summary>
        public FontUnderline Underline 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propUnderline;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Underline, out propUnderline);
                    return (FontUnderline)(uint)propUnderline.Value;
                }

                return _underline.GetValueOrDefault(FontUnderline.NotAvailable);
            }
            set
            {
                _underline = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Strikethrough property
        /// </summary>
        public FontProperties Strikethrough 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propStrikethrough;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_Strikethrough, out propStrikethrough);
                    return (FontProperties)(uint)propStrikethrough.Value;
                }

                return _strikethrough.GetValueOrDefault(RibbonLib.Interop.FontProperties.NotAvailable);
            }
            set
            {
                _strikethrough = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Foreground color property
        /// </summary>
        public Color ForegroundColor 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propForegroundColorType;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_ForegroundColorType, out propForegroundColorType);
                    SwatchColorType swatchColorType = (SwatchColorType)(uint)propForegroundColorType.Value;

                    switch (swatchColorType)
                    {
                        case SwatchColorType.RGB:
                            PropVariant propForegroundColor;
                            hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_ForegroundColor, out propForegroundColor);
                            return ColorTranslator.FromWin32((int)(uint)propForegroundColor.Value);

                        case SwatchColorType.Automatic:
                            return SystemColors.WindowText;

                        case SwatchColorType.NoColor:
                            throw new NotSupportedException("NoColor is not a valid value for ForegroundColor property in FontControl.");
                    }

                    return SystemColors.WindowText;
                }

                return _foregroundColor.GetValueOrDefault(SystemColors.WindowText);
            }
            set
            {
                _foregroundColor = value;
                _foregroundColorType = SwatchColorType.RGB;

                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Background color property
        /// </summary>
        public Color BackgroundColor 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propBackgroundColorType;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_BackgroundColorType, out propBackgroundColorType);
                    SwatchColorType swatchColorType = (SwatchColorType)(uint)propBackgroundColorType.Value;

                    switch (swatchColorType)
                    {
                        case SwatchColorType.RGB:
                            PropVariant propBackgroundColor;
                            hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_BackgroundColor, out propBackgroundColor);
                            return ColorTranslator.FromWin32((int)(uint)propBackgroundColor.Value);

                        case SwatchColorType.Automatic:
                            throw new NotSupportedException("Automatic is not a valid value for BackgroundColor property in FontControl.");

                        case SwatchColorType.NoColor:
                            return SystemColors.Window;
                    }

                    return SystemColors.Window;
                }

                return _backgroundColor.GetValueOrDefault(SystemColors.Window);
            }
            set
            {
                _backgroundColor = value;
                _backgroundColorType = SwatchColorType.RGB;

                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        /// <summary>
        /// Vertical positioning property
        /// </summary>
        public FontVerticalPosition VerticalPositioning 
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    IPropertyStore propertyStore = FontProperties;
                    PropVariant propVerticalPositioning;
                    HRESULT hr = propertyStore.GetValue(ref RibbonProperties.FontProperties_VerticalPositioning, out propVerticalPositioning);
                    return (FontVerticalPosition)(uint)propVerticalPositioning.Value;
                }

                return _verticalPositioning.GetValueOrDefault(FontVerticalPosition.NotAvailable);
            }
            set
            {
                _verticalPositioning = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.AllProperties, null);
                }
            }
        }

        #endregion
    }
}
