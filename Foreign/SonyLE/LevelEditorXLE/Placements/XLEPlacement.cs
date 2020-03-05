// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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

namespace LevelEditorXLE.Placements
{
    using PlcmtST = Schema.placementObjectType;

    public class Bookmark : DomNodeAdapter
    {
        public Uri Model
        {
            get { return GetAttribute<Uri>(Schema.modelBookmarkType.ModelAttribute); }
            set { SetAttribute(Schema.modelBookmarkType.ModelAttribute, value); }
        }

        public Uri Material
        {
            get { return GetAttribute<Uri>(Schema.modelBookmarkType.MaterialAttribute); }
            set { SetAttribute(Schema.modelBookmarkType.MaterialAttribute, value); }
        }

        public string Supplements
        {
            get { return GetAttribute<string>(Schema.modelBookmarkType.SupplementsAttribute); }
            set { SetAttribute(Schema.modelBookmarkType.SupplementsAttribute, value); }
        }
    }

    class XLEPlacementObject 
        : DomNodeAdapter
        , IVisible, ILockable, IListable, IReference<IResource>
    {
        public XLEPlacementObject() { }
        
        public Uri Model
        {
            get { return GetAttribute<Uri>(PlcmtST.modelAttribute); }
            set { SetAttribute(PlcmtST.modelAttribute, value); }
        }

        public Uri Material
        {
            get { return GetAttribute<Uri>(PlcmtST.materialAttribute); }
            set { SetAttribute(PlcmtST.materialAttribute, value); }
        }

        public string Supplements
        {
            get { return GetAttribute<string>(PlcmtST.supplementsAttribute); }
            set { SetAttribute(PlcmtST.supplementsAttribute, value); }
        }

        public static XLEPlacementObject Create()
        {
            return new DomNode(PlcmtST.Type).As<XLEPlacementObject>();
        }

        public static XLEPlacementObject Create(IResource target)
        {
            if (!CanReference(PlcmtST.Type, target))
                return null;
            
            var obj = new DomNode(PlcmtST.Type).As<XLEPlacementObject>();
            obj.Target = target;
            return obj;
        }

        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            try
            {
                info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
                info.Label = "<" + Path.GetFileNameWithoutExtension(Model.OriginalString) + ">";

                if (IsLocked)
                    info.StateImageIndex = info.GetImageList().Images.IndexOfKey(Sce.Atf.Resources.LockImage);
            }
            catch (Exception e)
            {
                info.Label = "Exception during GetInfo: " + e.Message;
            }
        }
        #endregion
        #region IVisible Members
        public virtual bool Visible
        {
            get { return GetAttribute<bool>(Schema.abstractPlacementObjectType.visibleAttribute) && this.AncestorIsVisible(); }
            set { SetAttribute(Schema.abstractPlacementObjectType.visibleAttribute, value); }
        }
        #endregion
        #region ILockable Members
        public virtual bool IsLocked
        {
            get { return GetAttribute<bool>(Schema.abstractPlacementObjectType.lockedAttribute) || this.AncestorIsLocked(); }
            set { SetAttribute(Schema.abstractPlacementObjectType.lockedAttribute, value); }
        }
        #endregion
        #region IReference Members
        public bool CanReference(IResource item)
        {
            if (item == null) return false;
            if (DomNode == null || DomNode.Type == null) return false;

            return CanReference(DomNode.Type, item);
        }
        public IResource Target
        {
            get { return null; }
            set 
            {
                System.Diagnostics.Debug.Assert(CanReference(value));
                if (value.Type == "ModelBookmark")
                {
                        // If this is a bookmark, we need to load the xml and 
                        // extract the key properties from there.
                    var bookmark = LoadBookmark(value.Uri);
                    Model = bookmark.Model;
                    Material = bookmark.Material;
                    Supplements = bookmark.Supplements ?? "";
                }
                else
                {
                    Model = value.Uri;
                    Material = value.Uri; 
                }
            }
        }
        private static bool CanReference(DomNodeType domtype, IResource resource)
        {
            var restraintType = domtype.GetTag(Annotations.ReferenceConstraint.ResourceType) as string;
            if (restraintType == null) return false;
            var types = restraintType.Split(',');
            return types.Where(x => string.Equals(x, resource.Type, StringComparison.OrdinalIgnoreCase)).FirstOrDefault() != null;
        }
        #endregion

        public static Bookmark LoadBookmark(Uri uri)
        {
            using (var stream = File.OpenRead(uri.LocalPath))
            {
                var schemaLoader = Globals.MEFContainer.GetExport<ISchemaLoader>().Value;
                var reader = new CustomDomXmlReader(uri, schemaLoader as XmlSchemaTypeLoader);
                var opaqueNode = reader.Read(stream, uri);
                return opaqueNode.As<Bookmark>();
            }
        }

        public static IQueryPredicate CreateSearchPredicate(Uri modelName)
        {
            // we need to build a predicate that looks for placements that are using this model
            var predicate = new Sce.Atf.Dom.DomNodePropertyPredicate();
            predicate.AddPropertyNameExpression("Model");
            var searchName = Path.GetFileNameWithoutExtension(modelName.OriginalString);
            predicate.AddValueStringSearchExpression(searchName, (UInt64)StringQuery.Contains, false);
            return predicate;
        }

        public static IQueryPredicate CreateSearchPredicate(Bookmark bkmrk, Uri baseUri)
        {
            // note -- we will only compare the "model"
            var predicate = new Sce.Atf.Dom.DomNodePropertyPredicate();
            predicate.AddPropertyNameExpression("Model");
            var searchName = Path.GetFileNameWithoutExtension(bkmrk.Model.OriginalString);
            predicate.AddValueStringSearchExpression(searchName, (UInt64)StringQuery.Matches, false);
            return predicate;
        }
    }
}
