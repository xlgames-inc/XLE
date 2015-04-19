//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Runtime.InteropServices;

using Sce.Atf.Dom;
using Sce.Atf.Adaptation;

using LevelEditorCore;

namespace RenderingInterop
{
    /// <summary>
    /// Adapter for DomNode that have runtime counterpart.
    /// It hold native object id and pushes dom changes to native object</summary>
    public class NativeObjectAdapter : DomNodeAdapter, INativeObject
    {
        protected override void OnNodeSet()
        {
            base.OnNodeSet();            
            DomNode node = DomNode;           
            node.AttributeChanged += node_AttributeChanged;            
            TypeId = (uint)DomNode.Type.GetTag(NativeAnnotations.NativeType);

            System.Diagnostics.Debug.Assert(m_instanceId==0);

            // <<XLE    
            //      moveing child initialisation to here (from 
            //      NativeGameEditor::m_gameDocumentRegistry_DocumentAdded)
            //      We need to initialise the children of
            //      non-game documents (but the previous path only worked for game documents)
            //
            //      Maybe neither is ideal. We should do this after a "resolve" event
            //      for the document. 
            CreateNativeObject();
            // XLE>>
        }

        public void OnRemoveFromDocument(NativeDocumentAdapter doc)
        {
            OnRemoveFromDocument_NonHier(doc);

                // note -- use "Subtree" & not "Children" because this
                //  will allow us to also touch NativeObjectAdapters
                //  that are children of non-native objects
                //  eg:
                //      <<native object>>
                //          <<non-native object>
                //              <<native object>>
            foreach (DomNode child in DomNode.Subtree)
            {
                NativeObjectAdapter childObject = child.As<NativeObjectAdapter>();
                if (childObject != null)
                    childObject.OnRemoveFromDocument_NonHier(doc);
            }
        }

        private void OnRemoveFromDocument_NonHier(NativeDocumentAdapter doc)
        {
            if (DomNode == null || m_instanceId == 0) return;

            ulong documentId = (doc != null) ? doc.NativeDocumentId : 0;
            System.Diagnostics.Debug.Assert(documentId == m_documentId);

            GameEngine.DestroyObject(m_documentId, m_instanceId, TypeId);
            GameEngine.DeregisterGob(m_documentId, m_instanceId, this);
            ReleaseNativeHandle();
        }

        public void OnAddToDocument(NativeDocumentAdapter doc)
        {
            var node = DomNode;
            var docTest = (node != null) ? node.GetRoot().As<NativeDocumentAdapter>() : null;
            System.Diagnostics.Debug.Assert(doc == docTest);
            CreateNativeObject();
        }

        public void OnSetParent(NativeObjectAdapter newParent, int insertionPosition)
        {
            if (newParent != null) {
                GameEngine.SetObjectParent(
                    m_documentId,
                    InstanceId, TypeId,
                    newParent.InstanceId, newParent.TypeId, insertionPosition);
            }
            else
            {
                GameEngine.SetObjectParent(
                    m_documentId,
                    InstanceId, TypeId,
                    0, 0, insertionPosition);
            }
        }
        
        void node_AttributeChanged(object sender, AttributeEventArgs e)
        {
            // process events only for the DomNode attached to this adapter.
            if (this.DomNode != e.DomNode)
                return;
            UpdateNativeProperty(e.AttributeInfo);
        }
        
        /// <summary>
        /// Updates all the shared properties  
        /// between this and native object 
        /// only call onece.
        /// </summary>
        public void UpdateNativeObject()
        {
            foreach (AttributeInfo attribInfo in this.DomNode.Type.Attributes)
            {
                UpdateNativeProperty(attribInfo);
            }
        }

        private void CreateNativeObject()
        {
            var node = DomNode;
            if (DomNode == null || m_instanceId != 0) return;

            var doc = node.GetRoot().As<NativeDocumentAdapter>();
            if (doc != null && doc.ManageNativeObjectLifeTime)
            {
                // The object might have a pre-assigned instance id.
                // If so, we should attempt to use that same id.
                // This is important if we want the id to persist between sessions
                //      --  For example, we must reference placement objects through
                //          a GUID value that remains constant. We don't want to create
                //          new temporary ids for placements every time we create them,
                //          when the GUID value is more reliable

                ulong existingId = 0;
                var idField = DomNode.Type.GetAttributeInfo("ID");
                if (idField != null)
                {
                    var id = DomNode.GetAttribute(idField);
                    if (id is UInt64)
                    {
                        existingId = (UInt64)id;
                    }
                    else
                    {
                        string stringId = id as string;
                        if (stringId != null && stringId.Length > 0)
                        {
                            if (!UInt64.TryParse(stringId, out existingId))
                            {
                                existingId = GUILayer.Util.HashID(stringId);
                            }
                        }
                    }
                }

                m_instanceId = GameEngine.CreateObject(doc.NativeDocumentId, existingId, TypeId, IntPtr.Zero, 0);

                if (m_instanceId != 0)
                {
                    m_documentId = doc.NativeDocumentId;
                    GameEngine.RegisterGob(m_documentId, m_instanceId, this);

                    UpdateNativeObject();
                }
            }
        }
      
        unsafe private void UpdateNativeProperty(AttributeInfo attribInfo)
        {
                // the "attribInfo" given to us may not belong this this exact
                // object type. It might belong to a base class (or even, possibly, to a super class). 
                // We need to check for these cases, and remap to the exact attribute that
                // belongs our concrete class.
            var type = DomNode.Type;
            if (attribInfo.DefiningType != type) {
                attribInfo = DomNode.Type.GetAttributeInfo(attribInfo.Name);
                if (attribInfo == null) return;
            }

            object idObj = attribInfo.GetTag(NativeAnnotations.NativeProperty);
            if (idObj == null) return;
            uint id = (uint)idObj;
            if (this.InstanceId == 0)
                return;

            uint typeId = TypeId;
            Type clrType = attribInfo.Type.ClrType;
            Type elmentType = clrType.GetElementType();

            object data = this.DomNode.GetAttribute(attribInfo);
            if (clrType.IsArray && elmentType.IsPrimitive)
            {
                GCHandle pinHandle = new GCHandle();
                try
                {
                    pinHandle = GCHandle.Alloc(data, GCHandleType.Pinned);
                    IntPtr ptr = pinHandle.AddrOfPinnedObject();
                    GameEngine.SetObjectProperty(
                        typeId, m_documentId, InstanceId, id,
                        ptr, elmentType, attribInfo.Type.Length);
                }
                finally
                {
                    if (pinHandle.IsAllocated)
                        pinHandle.Free();
                }                
            }
            else
            {
                IntPtr ptr = IntPtr.Zero;
                Type elemType = null;
                int arrayCount = 1;
                if (clrType == typeof(string))
                {
                    string str = (string)data;
                    if (!string.IsNullOrEmpty(str))
                    {
                        fixed (char* chptr = str)
                        {
                            ptr = new IntPtr((void*)chptr);
                            GameEngine.SetObjectProperty(
                                typeId, m_documentId, InstanceId, id, 
                                ptr, typeof(char), str.Length);
                        }
                        return;
                    }
                }
                else if (clrType == typeof(bool))
                {
                    bool val = (bool)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(bool);
                }
                else if (clrType == typeof(byte))
                {
                    byte val = (byte)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(byte);
                }
                else if (clrType == typeof(sbyte))
                {
                    sbyte val = (sbyte)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(sbyte);
                }
                else if (clrType == typeof(short))
                {
                    short val = (short)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(short);
                }
                else if (clrType == typeof(ushort))
                {
                    ushort val = (ushort)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(ushort);
                }
                else if (clrType == typeof(int))
                {
                    int val = (int)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(int);
                }
                else if (clrType == typeof(uint))
                {
                    uint val = (uint)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(uint);
                }
                else if (clrType == typeof(long))
                {
                    long val = (long)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(long);
                }
                else if (clrType == typeof(ulong))
                {
                    ulong val = (ulong)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(ulong);
                }
                else if (clrType == typeof(float))
                {
                    float val = (float)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(float);
                }
                else if (clrType == typeof(double))
                {
                    double val = (double)data;
                    ptr = new IntPtr(&val);
                    elemType = typeof(double);
                }
                else if (clrType == typeof(System.Uri))
                {
                    if(data != null && !string.IsNullOrWhiteSpace(data.ToString()))
                    {
                        Uri uri = (Uri)data;                        
                        string str = uri.LocalPath;
                        fixed (char* chptr = str)
                        {
                            ptr = new IntPtr((void*)chptr);
                            GameEngine.SetObjectProperty(typeId, m_documentId, InstanceId, id, ptr, typeof(char), str.Length);
                        }
                        return;
                    }
                }
                else if (clrType == typeof(DomNode))
                {
                    // this is a 'reference' to an object
                    DomNode node = (DomNode)data;
                    NativeObjectAdapter nativeGob = node.As<NativeObjectAdapter>();
                    if(nativeGob != null)
                    {
                        ptr = new IntPtr((void*)nativeGob.InstanceId);
                        elemType = typeof(ulong);                        
                    }
                }

                if (elemType != null)
                {
                    GameEngine.SetObjectProperty(typeId, m_documentId, InstanceId, id, ptr, elemType, arrayCount);
                }
            }
        }

        public uint TypeId
        {
            get;
            private set;
            // get
            // {
            //     var node = DomNode;
            //     if (node != null)
            //     {
            //         var tag = node.Type.GetTag(NativeAnnotations.NativeType);
            //         if (tag != null) { return (uint)tag; }
            //     }
            //     return 0;
            // }
        }        

        /// <summary>
        /// this method is exclusively used by GameEngine class.                
        public void ReleaseNativeHandle() { m_instanceId = 0; m_documentId = 0; }

        private ulong m_instanceId = 0;
        private ulong m_documentId = 0;
        
        #region INativeObject Members
        public void InvokeFunction(string fn, IntPtr arg, out IntPtr retval)
        {
            GameEngine.InvokeMemberFn(InstanceId,fn, arg, out retval);
        }

        /// <summary>
        /// Gets native id for this instance of the C++ counter part of this object.</summary>
        public ulong InstanceId
        {
            get { return m_instanceId; }

        }

        public ulong DocumentId
        {
            get { return m_documentId; }
        }
        #endregion
    }
}
