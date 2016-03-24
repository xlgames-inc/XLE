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

    class VisibleLockable : DomNodeAdapter, IVisible, ILockable
    {
        public virtual bool Visible
        {
            get { return GetAttribute<bool>(Schema.visibleTransformObjectType.visibleAttribute) && this.AncestorIsVisible(); }
            set { SetAttribute(Schema.visibleTransformObjectType.visibleAttribute, value); }
        }
        public virtual bool IsLocked
        {
            get { return GetAttribute<bool>(Schema.visibleTransformObjectType.lockedAttribute) || this.AncestorIsLocked(); }
            set { SetAttribute(Schema.visibleTransformObjectType.lockedAttribute, value); }
        }
    }
}
