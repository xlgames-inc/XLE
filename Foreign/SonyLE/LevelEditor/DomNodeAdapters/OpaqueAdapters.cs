using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
    class OpaqueListable : DomNodeAdapter, IListable
    {
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = info.GetImageList().Images.IndexOfKey(LevelEditorCore.Resources.CubesImage);
            info.Label = DomNode.Type.GetTag("OpaqueListable") as string;
            info.Label = info.Label ?? "<<opaque>>";
            info.IsLeaf = false;
        }
    }
}
