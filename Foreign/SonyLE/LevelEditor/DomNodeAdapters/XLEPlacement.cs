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
    class XLEPlacementObject : GenericTransformableObject, IGameObject
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placement";
        }
        #endregion

        public XLEPlacementObject() : base(Schema.placementObjectType.transform) {}

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
