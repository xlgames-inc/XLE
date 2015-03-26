using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf.Adaptation;
using Sce.Atf.Dom;
using Sce.Atf.Applications;
using Sce.Atf.VectorMath;
using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    public class MaterialGob : GameObject, IPropertyEditingContext
    {
        IEnumerable<object> IPropertyEditingContext.Items
        {
            get
            {
                var un = Underlying;
                if (un != null)
                    return un.Items;
                return null;
            }
        }

        IEnumerable<System.ComponentModel.PropertyDescriptor> IPropertyEditingContext.PropertyDescriptors
        {
            get
            {
                var un = Underlying;
                if (un != null)
                    return un.PropertyDescriptors;
                return null;
            }
        }

        private XLELayer.MaterialLayer Underlying
        {
            get 
            { 
                if (_underlying==null) {
                    _underlying = new XLELayer.MaterialLayer("game/model/galleon/galleon.material:galleon_sail");
                }

                return _underlying;
            }
        }
        private XLELayer.MaterialLayer _underlying;

        public MaterialGob() { }

//        #region INameable
//        public virtual string Name { get; set; }
//        #endregion
//
//        #region ITransformable Members
//        public virtual void UpdateTransform() {}
//        public virtual Matrix4F Transform { get; set; }
//        public virtual Vec3F Translation { get; set; }
//        public virtual Vec3F Rotation { get; set; }
//        public virtual Vec3F Scale { get; set; }
//        public virtual Vec3F Pivot { get; set; }
//        public TransformationTypes TransformationType { get; set; }
//        #endregion
//
//        #region IVisible Members
//        public virtual bool Visible { get; set; }
//        #endregion
//
//        #region ILockable Members
//        public virtual bool IsLocked { get; set; }
//        #endregion
//
//        #region IListable Members
//        public virtual void GetInfo(ItemInfo info)
//        {
//            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
//            info.Label = Name;
//            if (IsLocked)
//                info.StateImageIndex = info.GetImageList().Images.IndexOfKey(Sce.Atf.Resources.LockImage);
//        }
//        #endregion
    }
}


