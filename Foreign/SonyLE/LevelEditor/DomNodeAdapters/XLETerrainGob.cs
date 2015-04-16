using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    class XLETerrainGob : DomNodeAdapter, IListable
    {
        public XLETerrainGob() {}
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Terrain";
        }
    }
}
