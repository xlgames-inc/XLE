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

    public class Bookmark
    {
        public string Model;
        public string Material;
        public string Supplements;
    }

    class XLEPlacementObject 
        : DomNodeAdapter
        , IVisible, ILockable, IListable, IReference<IResource>
    {
        public XLEPlacementObject() { }
        
        public string Model
        {
            get { return GetAttribute<string>(PlcmtST.modelAttribute); }
            set { SetAttribute(PlcmtST.modelAttribute, value); }
        }

        public string Material
        {
            get { return GetAttribute<string>(PlcmtST.materialAttribute); }
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
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "<" + Path.GetFileNameWithoutExtension(Model) + ">";
        }
        #endregion
        #region IVisible Members
        public virtual bool Visible
        {
            get { return GetAttribute<bool>(Schema.abstractPlacementObjectType.visibleAttribute); }
            set { SetAttribute(Schema.abstractPlacementObjectType.visibleAttribute, value); }
        }
        #endregion
        #region ILockable Members
        public virtual bool IsLocked
        {
            get
            {
                bool locked = GetAttribute<bool>(Schema.abstractPlacementObjectType.lockedAttribute);
                if (locked == false)
                {
                    ILockable lockable = GetParentAs<ILockable>();
                    if (lockable != null)
                        locked = lockable.IsLocked;
                }
                return locked;
            }
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
                    Model = AsReferenceName(ResolveBookmarkUri(bookmark.Model, value.Uri));
                    Material = AsReferenceName(ResolveBookmarkUri(bookmark.Material, value.Uri));
                    Supplements = bookmark.Supplements ?? "";
                }
                else
                {
                    var referenceName = AsReferenceName(value.Uri);
                    Model = referenceName;
                    Material = referenceName; 
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
        private static string AsReferenceName(Uri uri)
        {
            var resService = Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            return resService.StripExtension(resService.AsAssetName(uri));
        }

        private static Uri ResolveBookmarkUri(string target, Uri bookmarkFileUri)
        {
            // This path handling is really awkward. But there's no way around this.
            // Values in the bookmark files aren't true Uris, because they can contain parameters
            // after the filename. But we need to make them relative to the bookmark URI
            // filename. It would be better to use the robust c++ filename handling stuff for this
            // but that's also difficult.
            // The normal .net Uri functions don't seem to work exactly right, either -- because
            // our filenames are wierd.
            var basePath = bookmarkFileUri.AbsoluteUri;
            var lastSep = System.Math.Max(basePath.LastIndexOf('/'), basePath.LastIndexOf('\\'));
            if (lastSep != -1) basePath = basePath.Substring(0, lastSep + 1);
            return new System.Uri(basePath + target);
        }
        #endregion

        public static Bookmark LoadBookmark(Uri uri)
        {
            var stream = new System.IO.FileStream(
                uri.LocalPath,
                System.IO.FileMode.Open, System.IO.FileAccess.Read);
            var serializer = new System.Xml.Serialization.XmlSerializer(typeof(Bookmark));
            return serializer.Deserialize(stream) as Bookmark;
        }

        public static IQueryPredicate CreateSearchPredicate(Uri modelName)
        {
            // we need to build a predicate that looks for placements that are using this model
            var predicate = new Sce.Atf.Dom.DomNodePropertyPredicate();
            predicate.AddPropertyNameExpression("Model");
            predicate.AddValueStringSearchExpression(AsReferenceName(modelName), (UInt64)StringQuery.Matches, false);
            return predicate;
        }

        public static IQueryPredicate CreateSearchPredicate(Bookmark bkmrk, Uri baseUri)
        {
            // note -- we will only compare the "model"
            var predicate = new Sce.Atf.Dom.DomNodePropertyPredicate();
            predicate.AddPropertyNameExpression("Model");
            var refName = AsReferenceName(ResolveBookmarkUri(bkmrk.Model, baseUri));
            predicate.AddValueStringSearchExpression(refName, (UInt64)StringQuery.Matches, false);
            return predicate;
        }
    }
}
