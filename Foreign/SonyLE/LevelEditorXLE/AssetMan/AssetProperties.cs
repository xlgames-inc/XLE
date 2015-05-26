using System;
using System.ComponentModel;
using System.ComponentModel.Composition;
using System.Globalization;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Design;

namespace LevelEditorXLE
{
    public interface IXLEAssetService
    {
        string AsAssetName(Uri uri);
        string StripExtension(string input);
        string GetBaseTextureName(string input);
    };

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
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            return resService.AsAssetName(uri);
        }
    }

    public class AssetNameNoExtConverter : TypeConverter
    {
        protected virtual string UriToAssetName(Uri uri)
        {
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            return resService.StripExtension(resService.AsAssetName(uri));
        }
    }

    public class BaseTextureNameConverter : AssetNameConverter
    {
        protected override string UriToAssetName(Uri uri)
        {
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            return resService.GetBaseTextureName(resService.AsAssetName(uri));
        }
    }


    [Export(typeof(IXLEAssetService))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLEAssetService : IXLEAssetService
    {
        public virtual string AsAssetName(Uri uri)
        {
            // covert this uri into a string filename that is fit for the assets system
            if (uri.IsAbsoluteUri)
            {
                var cwd = new Uri(System.IO.Directory.GetCurrentDirectory().TrimEnd('\\') + "\\");
                var relUri = cwd.MakeRelativeUri(uri);
                return Uri.UnescapeDataString(relUri.OriginalString);
            }
            else
            {
                return Uri.UnescapeDataString(uri.OriginalString);
            }
        }

        public virtual string StripExtension(string input)
        {
            int dot = input.LastIndexOf('.');
            int sep0 = input.LastIndexOf('/');
            int sep1 = input.LastIndexOf('\\');
            if (dot > 0 && dot > sep0 && dot > sep1)
            {
                return input.Substring(0, dot);
            }
            return input;
        }

        public virtual string GetBaseTextureName(string input)
        {
            // to get the "base texture name", we must strip off _ddn, _df and _sp suffixes
            var withoutExt = StripExtension(input);
            if (withoutExt.EndsWith("_ddn", true, System.Globalization.CultureInfo.CurrentCulture))
            {
                return withoutExt.Substring(0, withoutExt.Length - 4);
            }
            if (withoutExt.EndsWith("_df", true, System.Globalization.CultureInfo.CurrentCulture)
                || withoutExt.EndsWith("_sp", true, System.Globalization.CultureInfo.CurrentCulture))
            {
                return withoutExt.Substring(0, withoutExt.Length - 3);
            }
            return withoutExt;
        }
    };

    
}
