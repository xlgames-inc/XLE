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
    class XLEPlacementObject : GenericTransformableObject, IListable
    {
        #region IListable Members
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Placement";
        }
        #endregion

        public XLEPlacementObject() : base(Schema.placementObjectType.transform) {}

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
