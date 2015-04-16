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
    class XLEPlacementObject : GenericTransformableObject, IGameObject, IReference<IResource>
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "<" + Path.GetFileNameWithoutExtension(Model) + ">";
        }
        #endregion

        public XLEPlacementObject() : base(Schema.abstractPlacementObjectType.transform) {}

        #region INameable Members
        public string Name
        {
            get { return "Placement"; }
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

        public string Model
        {
            get 
            {
                return GetAttribute<string>(Schema.placementObjectType.modelChild);
            }
        }
        public string Material
        {
            get
            {
                return GetAttribute<string>(Schema.placementObjectType.materialChild);
            }
        }

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

                SetAttribute(Schema.placementObjectType.modelChild, referenceName);
                SetAttribute(Schema.placementObjectType.materialChild, referenceName); 
            }
        }
        private static bool CanReference(DomNodeType domtype, IResource resource)
        {
            var type = domtype.GetTag(Annotations.ReferenceConstraint.ResourceType) as string;
            if (type != null) {
                return type == resource.Type;
            }
            return false;
        }

        public static XLEPlacementObject Create()
        {
            return new DomNode(Schema.placementObjectType.Type).As<XLEPlacementObject>();
        }
    }
}
