//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Runtime.InteropServices;

using Sce.Atf.Dom;
using Sce.Atf.Adaptation;

using LevelEditorCore;

using System.Collections.Generic;

namespace RenderingInterop
{
    using PropertyInitializer = GUILayer.EditorSceneManager.PropertyInitializer;

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

            // the "attribInfo" given to us may not belong this this exact
            // object type. It might belong to a base class (or even, possibly, to a super class). 
            // We need to check for these cases, and remap to the exact attribute that
            // belongs our concrete class.
            var type = DomNode.Type;
            var attribInfo = e.AttributeInfo;
            if (attribInfo.DefiningType != type)
            {
                attribInfo = DomNode.Type.GetAttributeInfo(attribInfo.Name);
                if (attribInfo == null) return;
            }

            var properties = new List<PropertyInitializer>();
            var handles = new List<GCHandle>();

            unsafe
            {
                UpdateNativeProperty(attribInfo, properties, handles);

                if (properties.Count > 0)
                {
                    GameEngine.SetObjectProperty(
                        TypeId, DocumentId, InstanceId,
                        properties);
                }

                foreach (var i in handles)
                {
                    i.Free();
                }
            }
        }
        
        /// <summary>
        /// Updates all the shared properties  
        /// between this and native object 
        /// only call onece.
        /// </summary>
        public void UpdateNativeObject()
        {
            var properties = new List<PropertyInitializer>();
            var handles = new List<GCHandle>();

            try
            {
                foreach (AttributeInfo attribInfo in this.DomNode.Type.Attributes)
                    UpdateNativeProperty(attribInfo, properties, handles);

                if (properties.Count > 0)
                {
                    GameEngine.SetObjectProperty(
                        TypeId, DocumentId, InstanceId,
                        properties);
                }
            }
            finally
            {
                foreach (var i in handles) i.Free();
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

                var properties = new List<PropertyInitializer>();
                var handles = new List<GCHandle>();

                try
                {
                    foreach (AttributeInfo attribInfo in this.DomNode.Type.Attributes)
                        UpdateNativeProperty(attribInfo, properties, handles);

                    m_instanceId = GameEngine.CreateObject(doc.NativeDocumentId, existingId, TypeId, properties);
                }
                finally
                {
                    foreach (var i in handles) i.Free();
                }

                if (m_instanceId != 0)
                {
                    m_documentId = doc.NativeDocumentId;
                    GameEngine.RegisterGob(m_documentId, m_instanceId, this);
                }
            }
        }
      
        unsafe private static void SetStringProperty(
            uint propId, string str,
            IList<PropertyInitializer> properties,
            IList<GCHandle> handles)
        {
            if (!string.IsNullOrEmpty(str))
            {
                GCHandle pinHandle = GCHandle.Alloc(str, GCHandleType.Pinned);
                handles.Add(pinHandle);

                properties.Add(GameEngine.CreateInitializer(
                    propId, pinHandle.AddrOfPinnedObject(), 
                    typeof(char), (uint)str.Length));
            }
        }

        unsafe private static void SetBasicProperty<T>(
            uint propId, T obj,
            IList<PropertyInitializer> properties,
            IList<GCHandle> handles)
        {
            GCHandle pinHandle = GCHandle.Alloc(obj, GCHandleType.Pinned);
            handles.Add(pinHandle);

            properties.Add(GameEngine.CreateInitializer(
                propId, pinHandle.AddrOfPinnedObject(), 
                typeof(T), 1));
        }

        unsafe private void UpdateNativeProperty(
            AttributeInfo attribInfo,
            IList<PropertyInitializer> properties,
            IList<GCHandle> handles)
        {
            object idObj = attribInfo.GetTag(NativeAnnotations.NativeProperty);
            if (idObj == null) return;
            uint id = (uint)idObj;

            Type clrType = attribInfo.Type.ClrType;
            Type elmentType = clrType.GetElementType();

            object data = this.DomNode.GetAttribute(attribInfo);
            if (clrType.IsArray && elmentType.IsPrimitive)
            {
                GCHandle pinHandle = GCHandle.Alloc(data, GCHandleType.Pinned);
                handles.Add(pinHandle);

                properties.Add(GameEngine.CreateInitializer(
                    id, pinHandle.AddrOfPinnedObject(), 
                    elmentType, (uint)attribInfo.Type.Length));
            }
            else
            {
                if (clrType == typeof(string))      SetStringProperty(id, (string)data, properties, handles);
                else if (clrType == typeof(bool))   SetBasicProperty(id, Convert.ToUInt32((bool)data), properties, handles);
                else if (clrType == typeof(byte))   SetBasicProperty(id, (byte)data, properties, handles);
                else if (clrType == typeof(sbyte))  SetBasicProperty(id, (sbyte)data, properties, handles);
                else if (clrType == typeof(short))  SetBasicProperty(id, (short)data, properties, handles);
                else if (clrType == typeof(ushort)) SetBasicProperty(id, (ushort)data, properties, handles);
                else if (clrType == typeof(int))    SetBasicProperty(id, (int)data, properties, handles);
                else if (clrType == typeof(uint))   SetBasicProperty(id, (uint)data, properties, handles);
                else if (clrType == typeof(long))   SetBasicProperty(id, (long)data, properties, handles);
                else if (clrType == typeof(ulong))  SetBasicProperty(id, (ulong)data, properties, handles);
                else if (clrType == typeof(float))  SetBasicProperty(id, (float)data, properties, handles);
                else if (clrType == typeof(double)) SetBasicProperty(id, (double)data, properties, handles);
                else if (clrType == typeof(System.Uri))
                {
                    if(data != null && !string.IsNullOrWhiteSpace(data.ToString()))
                    {
                        Uri uri = (Uri)data;                        
                        string str = uri.LocalPath;
                        SetStringProperty(id, str, properties, handles);
                    }
                }
                else if (clrType == typeof(DomNode))
                {
                    // this is a 'reference' to an object
                    DomNode node = (DomNode)data;
                    NativeObjectAdapter nativeGob = node.As<NativeObjectAdapter>();
                    if(nativeGob != null)
                    {
                        SetBasicProperty(id, (ulong)nativeGob.InstanceId, properties, handles);
                    }
                }
            }
        }

        public uint TypeId
        {
            get;
            private set;
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
