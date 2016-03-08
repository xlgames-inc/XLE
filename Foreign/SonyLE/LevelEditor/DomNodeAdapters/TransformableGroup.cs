//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.Collections.Generic;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;
using LevelEditorCore.VectorMath;

namespace LevelEditor.DomNodeAdapters
{
    /// <summary>
    /// DomNodeAdapter for game object groups</summary>
    public class TransformableGroup : TransformObject, ITransformableGroup, IListable, IBoundable, IVisible, ILockable
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            
            // Register child events
            DomNode.ChildInserted += DomNode_ChildInserted;
            DomNode.ChildRemoved += DomNode_ChildRemoved;                      
            CheckScaleFlags();
        }

        private void DomNode_ChildInserted(object sender, ChildEventArgs e)
        {
            if (e.Parent == DomNode)
            {
                CheckScaleFlags();
            }
           
        }

        private void DomNode_ChildRemoved(object sender, ChildEventArgs e)
        {
            if (e.Parent == DomNode)
            {
                CheckScaleFlags();
            }           
        }

        private void CheckScaleFlags()
        {
            ITransformable thisNode = this.As<ITransformable>();
            if (thisNode == null)
                return;

            // Assume that Scale is set and UniformScale is not set until we know otherwise.
            TransformationTypes newFlags = thisNode.TransformationType;
            newFlags |= TransformationTypes.Scale;
            newFlags &= ~TransformationTypes.UniformScale;

            IEnumerable<ITransformable> transformables = GetChildList<ITransformable>(Schema.gameObjectGroupType.gameObjectChild);
            foreach (ITransformable childNode in transformables)
            {
                if ((childNode.TransformationType & TransformationTypes.Scale) == 0)
                        newFlags &= ~(TransformationTypes.Scale);
                if ((childNode.TransformationType & TransformationTypes.UniformScale) != 0)
                        newFlags |= TransformationTypes.UniformScale;                                
            }

            thisNode.TransformationType = newFlags;
        }

        /// <summary>
        /// Gets game object children</summary>
        public IList<ITransformable> Objects
        {
            get { return GetChildList<ITransformable>(Schema.gameObjectGroupType.gameObjectChild); }
        }

        #region IHierarchical Members

        public bool CanAddChild(object child)
        {
            return (child.Is<ITransformable>());
        }

        public bool AddChild(object child)
        {
            var gameObject = child.As<ITransformable>();
            if (gameObject == null)
                return false;
            Objects.Add(gameObject);
            return true;            
        }

        #endregion

        #region IListable Members

        /// <summary>
        /// Gets display info (label, icon, ...) for the ProjectLister and other controls</summary>
        /// <param name="info">Item info: passed in and modified by this method</param>
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = "Group";
            info.IsLeaf = Objects.Count == 0;

            // if (IsLocked)
            //     info.StateImageIndex = info.GetImageList().Images.IndexOfKey(Sce.Atf.Resources.LockImage);
        }

        #endregion

        #region IBoundable Members
        public AABB BoundingBox
        {
            get
            {
                AABB groupBox = new AABB();
                foreach (var obj in Objects.AsIEnumerable<IBoundable>())
                    groupBox.Extend(obj.BoundingBox);
                return groupBox;
            }
        }

        public AABB LocalBoundingBox
        {
            get
            {
                AABB groupBox = new AABB();
                foreach (var obj in Objects.AsIEnumerable<IBoundable>()) {
                    var trans = obj.As<ITransformable>();
                    var xform = (trans!=null) ? trans.Transform : Sce.Atf.VectorMath.Matrix4F.Identity;
                    var bb = new AABB(obj.BoundingBox.Min, obj.BoundingBox.Max);
                    bb.Transform(xform);
                    groupBox.Extend(bb);
                }
                return groupBox;
            }
        }
        #endregion

        #region ILockable Members
        public bool IsLocked
        {
            get { return false; }
            set { }
        }
        #endregion

        #region IVisible Members
        public bool Visible
        {
            get { return true; }
            set {}
        }
        #endregion

    }
}
