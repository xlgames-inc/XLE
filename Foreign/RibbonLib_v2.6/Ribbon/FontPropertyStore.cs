//*****************************************************************************
//
//  File:       FontPropertyStore.cs
//
//  Contents:   Helper class that wraps an IPropertyStore interface that 
//              contains font properties
//
//*****************************************************************************

using System;
using System.Drawing;
using RibbonLib.Interop;

namespace RibbonLib
{
    public class FontPropertyStore
    {
        private IPropertyStore _propertyStore;

        public FontPropertyStore(IPropertyStore propertyStore)
        {
            if (propertyStore == null)
            {
                throw new ArgumentException("Parameter propertyStore cannot be null.", "propertyStore");
            }
            _propertyStore = propertyStore;
        }

        public string Family
        {
            get
            {
                PropVariant propFamily;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Family, out propFamily);
                return (string)propFamily.Value;
            }
        }

        public decimal Size
        {
            get
            {
                PropVariant propSize;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Size, out propSize);
                return (decimal)propSize.Value;
            }
        }

        public FontProperties Bold
        {
            get
            {
                PropVariant propBold;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Bold, out propBold);
                return (FontProperties)(uint)propBold.Value;
            }
        }

        public FontProperties Italic
        {
            get
            {
                PropVariant propItalic;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Italic, out propItalic);
                return (FontProperties)(uint)propItalic.Value;
            }
        }

        
        public FontUnderline Underline
        {
            get
            {
                PropVariant propUnderline;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Underline, out propUnderline);
                return (FontUnderline)(uint)propUnderline.Value;
            }
        }

        public FontProperties Strikethrough
        {
            get
            {
                PropVariant propStrikethrough;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_Strikethrough, out propStrikethrough);
                return (FontProperties)(uint)propStrikethrough.Value;
            }
        }

        public Color ForegroundColor
        {
            get
            {
                PropVariant propForegroundColor;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_ForegroundColor, out propForegroundColor);
                return ColorTranslator.FromWin32((int)(uint)propForegroundColor.Value);
            }
        }

        public SwatchColorType ForegroundColorType
        {
            get
            {
                PropVariant propForegroundColorType;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_ForegroundColorType, out propForegroundColorType);
                return (SwatchColorType)(uint)propForegroundColorType.Value;
            }
        }

        public FontDeltaSize DeltaSize
        {
            get
            {
                PropVariant propDeltaSize;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_DeltaSize, out propDeltaSize);
                return (FontDeltaSize)(uint)propDeltaSize.Value;
            }
        }

        public Color BackgroundColor
        {
            get
            {
                PropVariant propBackgroundColor;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_BackgroundColor, out propBackgroundColor);
                return ColorTranslator.FromWin32((int)(uint)propBackgroundColor.Value);
            }
        }

        public SwatchColorType BackgroundColorType
        {
            get
            {
                PropVariant propBackgroundColorType;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_BackgroundColorType, out propBackgroundColorType);
                return (SwatchColorType)(uint)propBackgroundColorType.Value;
            }
        }

        public FontVerticalPosition VerticalPositioning
        {
            get
            {
                PropVariant propVerticalPositioning;
                HRESULT hr = _propertyStore.GetValue(ref RibbonProperties.FontProperties_VerticalPositioning, out propVerticalPositioning);
                return (FontVerticalPosition)(uint)propVerticalPositioning.Value;
            }
        }

    }
}
