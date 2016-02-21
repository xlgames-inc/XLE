//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using Sce.Atf.Dom;
using Sce.Atf.Adaptation;

namespace RenderingInterop
{
    public class NativeDocumentAdapter : DomNodeAdapter, XLEBridgeUtils.INativeDocumentAdapter
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
                var childObject = subnode.As<XLEBridgeUtils.INativeObjectAdapter>();
                if (childObject != null)
                {
                    childObject.OnAddToDocument(this);

                    var parentObject = subnode.Parent.As<XLEBridgeUtils.INativeObjectAdapter>();
                    if (parentObject != null)
                        childObject.OnSetParent(parentObject, -1);
                }
            }
            
            node.ChildInserted += node_ChildInserted;
            node.ChildRemoved += node_ChildRemoved;
        }

        public void OnDocumentRemoved()
        {
                // we need to call OnRemoveFromDocument on all children
                // NativeObjectAdapter.OnRemoveFromDocument is hierarchical,
                // so we only need to call on the top level children
            DomNode node = this.DomNode; 
            if (node != null)
            {
                foreach (var subnode in node.Children)
                {
                    var childObject = subnode.As<XLEBridgeUtils.INativeObjectAdapter>();
                    if (childObject != null)
                        childObject.OnRemoveFromDocument(this);
                }
            }

                // destroy the document on the native side, as well
            var tag = node.Type.GetTag(NativeAnnotations.NativeDocumentType);
            var typeId = (tag != null) ? (uint)tag : 0;
            GameEngine.DeleteDocument(m_nativeDocId, typeId);
        }

        void node_ChildInserted(object sender, ChildEventArgs e)
        {
            node_ChildInserted_Internal(e.Child, e.Parent, e.Index);
        }

        private void node_ChildInserted_Internal(object child, object parent, int insertionPoint)
        {
            var childObject = child.As<XLEBridgeUtils.INativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnAddToDocument(this);

                var parentObject = parent.As<XLEBridgeUtils.INativeObjectAdapter>();
                if (parentObject != null)
                {
                    childObject.OnSetParent(parentObject, insertionPoint);
                }
                else
                {
                    childObject.OnSetParent(null, -1);
                }
            }

            var children = child.As<DomNode>().Children;
            int index = 0;
            foreach (var c in children) {
                node_ChildInserted_Internal(c, child, index);
                ++index;
            }
        }

        void node_ChildRemoved(object sender, ChildEventArgs e)
        {
            AttemptRemoveNative(e.Child);
        }

        private void AttemptRemoveNative(DomNode node)
        {
            var childObject = node.As<NativeObjectAdapter>();
            if (childObject != null)
            {
                childObject.OnSetParent(null, -1);
                childObject.OnRemoveFromDocument(this);
            }
            else
            {
                // it might have native children, so we have to search for them
                foreach (var child in node.Children)
                    AttemptRemoveNative(child);
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
