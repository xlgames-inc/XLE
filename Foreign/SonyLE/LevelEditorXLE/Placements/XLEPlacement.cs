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

    class XLEPlacementObject 
        : DomNodeAdapter
        , INameable, IVisible, ILockable, IListable, IReference<IResource>
    {
        public XLEPlacementObject() { }
        
        public string Model
        {
            get { return GetAttribute<string>(PlcmtST.modelAttribute); }
        }

        public string Material
        {
            get { return GetAttribute<string>(PlcmtST.materialAttribute); }
        }

        public static XLEPlacementObject Create()
        {
            return new DomNode(PlcmtST.Type).As<XLEPlacementObject>();
        }

        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = Name;
        }
        #endregion
        #region INameable Members
        public string Name
        {
            get { return "<" + Path.GetFileNameWithoutExtension(Model) + ">"; }
            set { }
        }
        #endregion
        #region IVisible Members
        public virtual bool Visible
        {
            get { return true; }
            set { }
        }
        #endregion
        #region ILockable Members
        public virtual bool IsLocked
        {
            get { return false; }
            set { }
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
            set {
                var resService = Globals.MEFContainer.GetExportedValue<XLELayer.IXLEAssetService>();
                var referenceName = resService.StripExtension(resService.AsAssetName(value.Uri));

                SetAttribute(PlcmtST.modelAttribute, referenceName);
                SetAttribute(PlcmtST.materialAttribute, referenceName); 
            }
        }
        public static bool CanReferenceStatic(IResource resource)
        {
            return CanReference(PlcmtST.Type, resource);
        }
        public static bool CanReference(DomNodeType domtype, IResource resource)
        {
            var type = domtype.GetTag(Annotations.ReferenceConstraint.ResourceType) as string;
            if (type != null) {
                return type == resource.Type;
            }
            return false;
        }
        #endregion
    }
}
