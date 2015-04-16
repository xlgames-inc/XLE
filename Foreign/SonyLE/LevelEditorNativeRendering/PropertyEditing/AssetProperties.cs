using System;
using System.ComponentModel;
using System.Globalization;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Design;

namespace RenderingInterop.PropertyEditing
{
    public class AssetNameConverter : TypeConverter
    {
        public override bool CanConvertFrom(ITypeDescriptorContext context, Type sourceType)
        {
            return (sourceType == typeof(string)) ? true : base.CanConvertFrom(context, sourceType);
        }

        public override object ConvertFrom(ITypeDescriptorContext context, CultureInfo culture, object value)
        {
            var uri = value as Uri;
            if (uri != null)
                return UriToAssetName(uri);

            var str = value as string;
            if (str != null)
            {
                if (Uri.TryCreate(str, UriKind.RelativeOrAbsolute, out uri))
                {
                    return UriToAssetName(uri);
                }
                return str;
            }

            return base.ConvertFrom(context, culture, value);
        }

        public override object ConvertTo(
            ITypeDescriptorContext context,
            CultureInfo culture,
            object value,
            Type destinationType)
        {
            return base.ConvertTo(context, culture, value, destinationType);
        }

        protected virtual string UriToAssetName(Uri uri)
        {
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<XLELayer.IXLEAssetService>();
            return resService.StripExtension(resService.AsAssetName(uri));
        }
    }

    public class BaseTextureNameConverter : AssetNameConverter
    {
        protected override string UriToAssetName(Uri uri)
        {
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<XLELayer.IXLEAssetService>();
            return resService.GetBaseTextureName(resService.AsAssetName(uri));
        }
    }

    
}
