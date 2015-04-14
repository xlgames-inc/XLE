//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using Sce.Atf.Dom;
using Sce.Atf.Adaptation;

namespace RenderingInterop
{
    public class NativeDocumentAdapter : DomNodeAdapter
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();

            // we must register this document and get an id for it
            DomNode node = this.DomNode; 
            var tag = node.Type.GetTag(NativeAnnotations.NativeDocumentType);
            var typeId = (tag != null) ? (uint)tag : 0;
            m_nativeDocId = GameEngine.CreateDocument(typeId);

            ManageNativeObjectLifeTime = true;
            foreach (var subnode in node.Subtree)
            {
                NativeObjectAdapter childObject = subnode.As<NativeObjectAdapter>();
                if (childObject != null)
                {
                    childObject.OnAddToDocument(this);

                    NativeObjectAdapter parentObject = subnode.Parent.As<NativeObjectAdapter>();
                    if (parentObject != null)
                        childObject.OnSetParent(parentObject, -1);
                }
            }
            
            node.ChildInserted += node_ChildInserted;
            node.ChildRemoved += node_ChildRemoved;
        }

        void node_ChildInserted(object sender, ChildEventArgs e)
        {
            NativeObjectAdapter childObject = e.Child.As<NativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnAddToDocument(this);

                NativeObjectAdapter parentObject = e.Parent.As<NativeObjectAdapter>();
                if (parentObject != null)
                {
                    childObject.OnSetParent(parentObject, e.Index);
                }
                else
                {
                    childObject.OnSetParent(null, -1);
                }
            }
        }

        void node_ChildRemoved(object sender, ChildEventArgs e)
        {
            NativeObjectAdapter childObject = e.Child.As<NativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnRemoveFromDocument(this);

                childObject.OnSetParent(null, -1);
            }
        }

        /// <summary>
        /// Gets/Sets whether this adapter  creates native object on child inserted 
        /// and deletes it on child removed.
        /// The defautl is true.
        /// </summary>
        public bool ManageNativeObjectLifeTime
        {
            get;
            set;
        }
        public ulong NativeDocumentId { get { return m_nativeDocId; } }
        private ulong m_nativeDocId;
    }
}
