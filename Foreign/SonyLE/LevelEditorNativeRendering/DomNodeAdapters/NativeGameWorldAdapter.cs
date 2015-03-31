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
            DomNode node = this.DomNode;
            
            node.ChildInserted += node_ChildInserted;
            node.ChildRemoved += node_ChildRemoved;
            ManageNativeObjectLifeTime = true;

                // we must register this document and get an id for it
            var tag = node.Type.GetTag(NativeAnnotations.NativeDocumentType);
            var typeId = (tag!=null) ? (uint)tag : 0;
            m_nativeDocId = GameEngine.CreateDocument(typeId);
        }

        void node_ChildInserted(object sender, ChildEventArgs e)
        {
            NativeObjectAdapter childObject = e.Child.As<NativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnAddToDocument(this);
            }
        }

        void node_ChildRemoved(object sender, ChildEventArgs e)
        {
            NativeObjectAdapter childObject = e.Child.As<NativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnRemoveFromDocument(this);
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
