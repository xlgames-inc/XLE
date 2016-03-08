//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using LevelEditorCore;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

namespace LevelEditor.DomNodeAdapters
{
    using TransformableST = Schema.transformObjectType;

    /// <summary>
    /// GameObject, a (usually) renderable 3D object in the level</summary>
    /// <remarks>Game objects have a transformation (translation/rotation/scale) and a bounding box
    /// The can be rendered in the DesignView and listed in the ProjectLister tree view.</remarks>
    public class GameObject : TransformObject, IGameObject
    {
        #region INameable

        /// <summary>
        /// Gets and sets the name</summary>
        public virtual string Name
        {
            get { 
                var result = GetAttribute<string>(Schema.gameObjectType.nameAttribute);
                if (string.IsNullOrEmpty(result)) return "<<unnamed>>";
                return result;
            }
            set { SetAttribute(Schema.gameObjectType.nameAttribute, value); }
        }

        #endregion

        #region IVisible Members
        /// <summary>
        /// Node visibility</summary>
        public virtual bool Visible
        {
            get { return GetAttribute<bool>(Schema.gameObjectType.visibleAttribute) && this.AncestorIsVisible(); }
            set { SetAttribute(Schema.gameObjectType.visibleAttribute, value); }
        }
        #endregion
        #region ILockable Members

        /// <summary>
        /// Gets and sets a value indicating if the DomNode is locked
        /// </summary>
        public virtual bool IsLocked
        {
            get { return GetAttribute<bool>(Schema.gameObjectType.lockedAttribute) || this.AncestorIsLocked(); }
            set { SetAttribute(Schema.gameObjectType.lockedAttribute, value); }
        }

        #endregion
        #region IListable Members

        /// <summary>
        /// Gets display info (label, icon, ...) for the ProjectLister and other controls</summary>
        /// <param name="info">Item info: passed in and modified by this method</param>
        public virtual void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = Name;                        
            if (IsLocked)
                info.StateImageIndex = info.GetImageList().Images.IndexOfKey(Sce.Atf.Resources.LockImage);            
        }
      
        #endregion          
    
        public GameObject() {}
    }
   
}
