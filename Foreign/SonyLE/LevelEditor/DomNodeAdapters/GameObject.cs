//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using LevelEditorCore;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditor.DomNodeAdapters.Extensions;

namespace LevelEditor.DomNodeAdapters
{
    using TransformableST = Schema.transformObjectType;

    public class GenericTransformableObject : DomNodeAdapter, ITransformable
    {
        #region ITransformable Members

        public virtual void UpdateTransform()
        {
            this.SetMatrix4x4(TransformableST.transformAttribute, TransformUtils.CalcTransform(this));
        }

        /// <summary>
        /// Gets and sets the local transformation matrix</summary>
        public virtual Matrix4F Transform
        {
            get { return this.GetMatrix4x4(TransformableST.transformAttribute); }
        }

        /// <summary>
        /// Gets and sets the node translation</summary>
        public virtual Vec3F Translation
        {
            get { return this.GetVec3(TransformableST.translateAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Translation) == 0)
                    return;
                this.SetVec3(TransformableST.translateAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node rotation</summary>
        public virtual Vec3F Rotation
        {
            get { return this.GetVec3(TransformableST.rotateAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Rotation) == 0)
                    return;
                this.SetVec3(TransformableST.rotateAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node scale</summary>
        public virtual Vec3F Scale
        {
            get { return this.GetVec3(TransformableST.scaleAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Scale) == 0
                    && (TransformationType & TransformationTypes.UniformScale) == 0)
                    return;

                this.SetVec3(TransformableST.scaleAttribute, value);
            }
        }

        /// <summary>
        /// Gets and sets the node scale pivot</summary>
        public virtual Vec3F Pivot
        {
            get { return this.GetVec3(TransformableST.pivotAttribute); }
            set
            {
                if ((TransformationType & TransformationTypes.Pivot) == 0)
                    return;
                this.SetVec3(TransformableST.pivotAttribute, value);
            }
        }

        /// <summary>
        /// Gets or sets the type of transformation this object can support. By default
        /// all transformation types are supported.</summary>
        public TransformationTypes TransformationType
        {
            get { return GetAttribute<TransformationTypes>(TransformableST.transformationTypeAttribute); }
            set
            {
                int v = (int)value;
                SetAttribute(TransformableST.transformationTypeAttribute, v);
            }
        }

        #endregion

        public GenericTransformableObject() {}
    }

    /// <summary>
    /// GameObject, a (usually) renderable 3D object in the level</summary>
    /// <remarks>Game objects have a transformation (translation/rotation/scale) and a bounding box
    /// The can be rendered in the DesignView and listed in the ProjectLister tree view.</remarks>
    public class GameObject : GenericTransformableObject, IGameObject
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
            get { return GetAttribute<bool>(Schema.gameObjectType.visibleAttribute); }
            set { SetAttribute(Schema.gameObjectType.visibleAttribute, value); }
        }
        #endregion
        #region ILockable Members

        /// <summary>
        /// Gets and sets a value indicating if the DomNode is locked
        /// </summary>
        public virtual bool IsLocked
        {
            get 
            { 
                bool locked =  GetAttribute<bool>(Schema.gameObjectType.lockedAttribute);
                if (locked == false)
                {
                    ILockable lockable = GetParentAs<ILockable>();
                    if(lockable != null) 
                        locked = lockable.IsLocked;                        
                }
                return locked;                    
            }
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
