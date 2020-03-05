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
        string AsAssetName(LevelEditorCore.ResourceDesc desc);
        string GetBaseTextureName(string input);
        Uri GetBaseTextureName(Uri input);
    };

    public class BaseTextureNameConverter : TypeConverter
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
            }

            return base.ConvertFrom(context, culture, value);
        }

        public override object ConvertTo(
            ITypeDescriptorContext context, CultureInfo culture,
            object value, Type destinationType)
        {
            return base.ConvertTo(context, culture, value, destinationType);
        }

        protected Uri UriToAssetName(Uri uri)
        {
            var resService = LevelEditorCore.Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            return resService.GetBaseTextureName(uri);
        }
    }


    [Export(typeof(IXLEAssetService))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLEAssetService : IXLEAssetService
    {
        public virtual string AsAssetName(Uri uri)
        {
            string result;
            // covert this uri into a string filename that is fit for the assets system
            if (uri.IsAbsoluteUri)
            {
                var relUri = Utils.CurrentDirectoryAsUri().MakeRelativeUri(uri);

                    // If the relative directory beings with a ".." (ie, the file is somewhere else
                    // on the drive, outside of the working folder) then let's pass the full absolute
                    // path. 
                    // This is not ideally. Really we should be using only absolute paths in the C#
                    // side, and leave the native side to deal with converting them to relative when
                    // necessary.
                if (relUri.OriginalString.Substring(0, 2) == "..")
                {
                    result = Uri.UnescapeDataString(uri.LocalPath); // (use LocalPath to avoid the file:// prefix)
                }
                else
                {
                    result = Uri.UnescapeDataString(relUri.OriginalString);
                }
            }
            else
            {
                result = Uri.UnescapeDataString(uri.OriginalString);
            }

            // awkwardly, we want to make the filename lowercase here; but leave the case there for the parameters
            var paramSeparator = result.LastIndexOf('?');
            if (paramSeparator > 0)
            {
                result = result.Substring(0, paramSeparator).ToLower() + ":" + result.Substring(paramSeparator + 1);
            }
            else
                result = result.ToLower();
            return result;
        }

        public virtual string AsAssetName(LevelEditorCore.ResourceDesc desc)
        {
            return desc.MountedName;
        }

        public virtual string GetBaseTextureName(string input)
        {
                // To get the "base texture name", we must strip off _ddn, _df and _sp suffixes
                // we will replace them with _*
                // Note that we want to keep extension
                // It's important that we use "_*", because this works best with the FileOpen dialog
                // When open the editor for this filename, the FileOpen dialog will default to
                // the previous value. If we use "_*", the pattern will actually match the right
                // textures in that dialog -- and it feels more natural to the user. Other patterns
                // won't match any files
            var pattern = new
                System.Text.RegularExpressions.Regex("(_[dD][fF])|(_[dD][dD][nN])|(_[sS][pP])|(_[rR])");
            return pattern.Replace(input, "_*");
        }

        public virtual Uri GetBaseTextureName(Uri input)
        {
            // To get the "base texture name", we must strip off _ddn, _df and _sp suffixes
            // we will replace them with _*
            // Note that we want to keep extension
            // It's important that we use "_*", because this works best with the FileOpen dialog
            // When open the editor for this filename, the FileOpen dialog will default to
            // the previous value. If we use "_*", the pattern will actually match the right
            // textures in that dialog -- and it feels more natural to the user. Other patterns
            // won't match any files
            var pattern = new
                System.Text.RegularExpressions.Regex("(_[dD][fF])|(_[dD][dD][nN])|(_[sS][pP])|(_[rR])");
            return new Uri(pattern.Replace(input.ToString(), "_*"));
        }
    };

    
}
