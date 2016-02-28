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
            set {
                var resService = Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
                var referenceName = resService.StripExtension(resService.AsAssetName(value.Uri));

                Model = referenceName;
                Material = referenceName; 
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
