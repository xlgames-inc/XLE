// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel.Composition;
using System.ComponentModel;
using System.Xml.Serialization;
using Sce.Atf;
using Sce.Atf.Adaptation;

using LevelEditorCore;

namespace LevelEditorXLE.Placements
{
    [Export(typeof(IResourceConverter))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceConverter : IResourceConverter
    {
        #region IResourceConverter Members

        public class Bookmark   // must be public to be serialized...?
        {
            public string Model;
            public string Material;
            public string Supplements;
        }

        IAdaptable IResourceConverter.Convert(IResource resource)
        {
            if (resource == null) return null;

            if (resource.Type == "ModelBookmark")
            {
                // {
                //     var stream2 = new System.IO.FileStream(
                //         "E:\\XLE\\temp.xml",
                //         System.IO.FileMode.CreateNew, System.IO.FileAccess.Write);
                //     var serializer2 = new XmlSerializer(typeof(Bookmark));
                //     Bookmark b = new Bookmark();
                //     b.Model = "ModelName"; b.Material = "MaterialName"; b.Supplements = "Supp";
                //     serializer2.Serialize(stream2, b);
                // }

                // If this is a bookmark, we need to load the xml and 
                // extract the key properties from there.
                var stream = new System.IO.FileStream(
                    resource.Uri.LocalPath, 
                    System.IO.FileMode.Open, System.IO.FileAccess.Read);
                var serializer = new XmlSerializer(typeof(Bookmark));
                var obj = serializer.Deserialize(stream) as Bookmark;
                var newPlacement = XLEPlacementObject.Create();

                    // we need to convert the strings into uris relative to the
                    // original filename.
                var resService = Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
                var basePath = resource.Uri.LocalPath;
                    //  all this path processing is a little non-ideal. It would be better
                    //  to drag these strings into C++ so we can use the reliable path
                    //  manipulation routines there.
                var lastSep = System.Math.Max(basePath.LastIndexOf('/'), basePath.LastIndexOf('\\'));
                if (lastSep != -1) basePath = basePath.Substring(0, lastSep+1);
                newPlacement.Model = resService.StripExtension(resService.AsAssetName(new System.Uri(basePath + obj.Model)));
                newPlacement.Material = resService.StripExtension(resService.AsAssetName(new System.Uri(basePath + obj.Material)));
                newPlacement.Supplements = obj.Supplements ?? "";

                return newPlacement;
            }
            else 
            if (XLEPlacementObject.CanReferenceStatic(resource))
            {
                var newPlacement = XLEPlacementObject.Create();
                newPlacement.Target = resource;
                return newPlacement;
            }

            return null;
        }

        #endregion
    }
}
