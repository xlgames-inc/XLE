using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    public class XLEPlacementDocument : DomDocument, IListable, IHierarchical
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placements";
        }
        #endregion
        #region IHierarchical Members
        public bool CanAddChild(object child)
        {
            return child.Is<XLEPlacementObject>();
        }

        public bool AddChild(object child)
        {
            var placement = child.As<XLEPlacementObject>();
            if (placement != null)
            {
                GetChildList<XLEPlacementObject>(Schema.placementsDocumentType.placementsChild).Add(placement);
                return true;
            }
            return false;
        }
        #endregion

        public static XLEPlacementDocument OpenOrCreate(Uri uri, SchemaLoader schemaLoader)
        {
            if (!uri.IsAbsoluteUri)
                return null;

            var docRegistry = Globals.MEFContainer.GetExportedValue<GenericDocumentRegistry>();
            var existing = docRegistry.FindDocument<XLEPlacementDocument>(uri);
            if (existing != null) return existing;

            string filePath = uri.LocalPath;

            DomNode rootNode = null;
            if (File.Exists(filePath))
            {
                // read existing document using custom dom XML reader
                using (FileStream stream = File.OpenRead(filePath))
                {
                    var reader = new CustomDomXmlReader(Globals.ResourceRoot, schemaLoader);
                    rootNode = reader.Read(stream, uri);
                }
            }
            else
            {
                // create new document by creating a Dom node of the root type defined by the schema                 
                rootNode = new DomNode(Schema.placementsDocumentType.Type, Schema.placementsDocumentRootElement);
            }

            var doc = rootNode.As<XLEPlacementDocument>();
            doc.Uri = uri;

            // Initialize Dom extensions now that the data is complete
            rootNode.InitializeExtensions();

            docRegistry.Add(doc);
            doc.Dirty = false;
            return doc;
        }

        public override string Type { get { return GameEditor.s_placementDocInfo.FileType; } }
    }

    class XLEPlacementObject : DomNodeAdapter, IListable
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placement";
        }
        #endregion

        // public string Model
        // {
        //     get 
        //     {
        //         return GetAttribute<string>(Schema.placementObjectType.modelChild);
        //     }
        // }
        // 
        // public string Material
        // {
        //     get
        //     {
        //         return GetAttribute<string>(Schema.placementObjectType.materialChild);
        //     }
        // }
    }
}
