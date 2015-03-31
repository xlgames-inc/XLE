//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Xml;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Runtime.ConstrainedExecution;
using System.Security;
using System.Linq;
using System.Threading;

using Sce.Atf;
using Sce.Atf.VectorMath;
using Sce.Atf.Dom;
using Sce.Atf.Adaptation;

using LevelEditorCore;

namespace RenderingInterop
{
    /// <summary>
    /// Exposes a minimum set of game-engine functionalities 
    /// for LevelEditor purpose.</summary>    
    [Export(typeof(IGameEngineProxy))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    [SuppressUnmanagedCodeSecurity()]
    public unsafe class GameEngine : DisposableObject, IGameEngineProxy, IInitializable
    {
        public GameEngine()
        {
            CriticalError = s_notInitialized;
            s_inist = this;
            GameEngine.Init();
        }
        #region IGameEngineProxy Members

        public EngineInfo Info
        {
            get { return m_engineInfo; }
        }

        public void SetGameWorld(IGame game) {}

        /// <summary>
        /// Sets active game world.</summary>
        /// <param name="game">Game world to set</param>
        // public void SetGameWorld(IGame game)
        // {
        //     NativeObjectAdapter nobject = game.Cast<NativeObjectAdapter>();
        //     NativeSetGameLevel(nobject.InstanceId);
        // }

        /// <summary>
        /// Updates game world</summary>
        /// <param name="ft">Frame time</param>
        /// <param name="updateType">Update type</param>        
        public void Update(FrameTime ft, UpdateType updateType)
        {
            // NativeUpdate(&ft, updateType);
        }

        public void WaitForPendingResources()
        {
            // NativeWaitForPendingResources();
        }
        #endregion

        #region IInitializable Members

        void IInitializable.Initialize()
        {
           
            
        }

        #endregion

        private EngineInfo m_engineInfo;        
        private void PopulateEngineInfo(string engineInfoStr)
        {
            if (m_engineInfo != null) return;
            if (!string.IsNullOrWhiteSpace(engineInfoStr))
                m_engineInfo = new EngineInfo(engineInfoStr);
        }

        #region initialize and shutdown


        private static GameEngine s_inist;
        private static GUILayer.EngineDevice s_engineDevice;
        private static GUILayer.SavedRenderResources s_savedRenderResources;
        private static GUILayer.EditorSceneManager s_underlyingScene;

        /// <summary>
        /// init game engine 
        /// call it one time during startup on the UI thread.</summary>        
        public static void Init()
        {
            s_syncContext = SynchronizationContext.Current;
            s_invalidateCallback = new InvalidateViewsDlg(InvalidateViews);
            try
            {
                GUILayer.EngineDevice.SetDefaultWorkingDirectory();
                s_engineDevice = new GUILayer.EngineDevice();
                s_engineDevice.AttachDefaultCompilers();
                s_savedRenderResources = new GUILayer.SavedRenderResources(s_engineDevice);
                s_underlyingScene = new GUILayer.EditorSceneManager();
                CriticalError = "";
                s_inist.PopulateEngineInfo(
                    @"<EngineInfo>
                        <SupportedResources>
                            <ResourceDescriptor Type='Model' Name='Model' Description='Model' Ext='.dae' />
                        </SupportedResources>
                    </EngineInfo>");

                // \todo -- hook up logging to "LogCallback"
            }
            catch (Exception e)
            {
                CriticalError = "Error while initialising engine device: " + e.Message;
            }
        }

        public static bool IsInError { get { return CriticalError.Length > 0; } }
        public static string CriticalError { get; private set; }

        public static GUILayer.EditorSceneManager GetEditorSceneManager() { return s_underlyingScene; }

        /// <summary>
        /// delete all the game object in native side.
        /// and reset the game to default.
        /// </summary>
        public static void Clear()
        {
            s_idToDomNode.Clear();
            // _gameLevel = 0;
            // NativeClear();
        }

        /// <summary>
        /// shutdown game engine.
        /// call it one time on application exit.
        /// </summary>
        public static void Shutdown()
        {
            while (s_idToDomNode.Count > 0)
            {
                DestroyObject(
                    s_idToDomNode.Keys.First().First,
                    s_idToDomNode.Keys.First().Second, 
                    s_idToDomNode.Values.First().TypeId);
            }
            s_idToDomNode.Clear();

            s_engineDevice.Dispose();
            s_engineDevice = null;
            CriticalError = s_notInitialized;
        }

        public static event EventHandler RefreshView = delegate { };

        #endregion

        #region call back for invalidating views.
        private delegate void InvalidateViewsDlg();
        private static InvalidateViewsDlg s_invalidateCallback;
        private static void InvalidateViews()
        {
            if (s_syncContext != null)
            {
                s_syncContext.Post(obj=>RefreshView(null, EventArgs.Empty),null);
            }
        }
        private static SynchronizationContext s_syncContext;
        #endregion

        #region log callbacks

        [UnmanagedFunctionPointer(CallingConvention.StdCall,CharSet = CharSet.Unicode)]
        private delegate void LogCallbackType(int messageType, string text);
        private static void LogCallback(int messageType, string text)
        {
            Console.Write(text);
            if (messageType == (int)OutputMessageType.Warning
                || messageType == (int)OutputMessageType.Error)
                Outputs.Write((OutputMessageType)messageType, text);                            
        }

        #endregion

        #region update and rendering

        // private static ulong _gameLevel;
        // public static void SetGameLevel(NativeObjectAdapter game) 
        // { 
        //     _gameLevel = game != null ? game.InstanceId : 0;
        // }
        // 
        // public static NativeObjectAdapter GetGameLevel()            
        // {
        //     if (_gameLevel != 0)
        //         return GetAdapterFromId(0, _gameLevel);
        //     else
        //         return null;
        // }

        // public static void SetGameLevel(NativeObjectAdapter game)
        // {
        //     ulong insId = game != null ? game.InstanceId : 0;
        //     NativeSetGameLevel(insId);
        // }
        // 
        // public static NativeObjectAdapter GetGameLevel()
        // {
        //     ulong insId = NativeGetGameLevel();
        //     if (insId != 0)
        //         return GetAdapterFromId(insId);
        //     else
        //         return null;
        // }

        // public static void Begin(ulong renderSurface, Matrix4F viewxform, Matrix4F projxfrom)
        // {
        //     fixed (float* ptr1 = &viewxform.M11, ptr2 = &projxfrom.M11)
        //     {
        //         NativeBegin(renderSurface, ptr1, ptr2);
        //     }
        // }
        // 
        // public static void RenderGame()
        // {
        //     NativeRenderGame();
        // }
        // 
        // public static bool SaveRenderSurfaceToFile(ulong renderSurface, string fileName)
        // {
        //     return NativeSaveRenderSurfaceToFile(renderSurface, fileName);
        // }
        // 
        // public static void End()
        // {
        //     NativeEnd();
        // }

        #endregion

        #region Object management.

        public static uint GetObjectTypeId(string className)
        {
            if (IsInError) return 0;
            uint id = NativeGetObjectTypeId(className);
            if (id == 0)
            {
                System.Diagnostics.Debug.WriteLine(className + " is not defined in runtime");
            }
            return id;
        }

        public static uint GetDocumentTypeId(string className)
        {
            if (IsInError) return 0;
            uint id = NativeGetDocumentTypeId(className);
            if (id == 0)
            {
                System.Diagnostics.Debug.WriteLine(className + " document type is not defined in runtime");
            }
            return id;
        }

        public static uint GetObjectPropertyId(uint typeId, string propertyName)
        {
            if (IsInError) return 0;
            uint propId = NativeGetObjectPropertyId(typeId, propertyName);
            if (propId == 0)
            {
                System.Diagnostics.Debug.WriteLine(propertyName + " is not defined for typeid " + typeId);
            }
            return propId;
        }

        public static uint GetObjectChildListId(uint typeId, string listName)
        {
            if (IsInError) return 0;
            uint propId = NativeGetObjectChildListId(typeId, listName);
            if (propId == 0)
            {
                System.Diagnostics.Debug.WriteLine(listName + " is not defined for typeid " + typeId);
            }
            return propId;
        }

        public static ulong CreateObject(ulong documentId, uint typeId, IntPtr data, int size)
        {
            return NativeCreateObject(documentId, 0, typeId, data, size);
        }

        public static ulong CreateObject(ulong documentId, ulong existingId, uint typeId, IntPtr data, int size)
        {
            return NativeCreateObject(documentId, existingId, typeId, data, size);
        }

        public static void DestroyObject(ulong documentId, ulong instanceId, uint typeId)
        {
            NativeDestroyObject(documentId, instanceId, typeId);
        }

        public static void RegisterGob(ulong documentId, ulong instanceId, NativeObjectAdapter gob)
        {
            s_idToDomNode.Add(new Pair<ulong, ulong>(documentId, instanceId), gob);
        }

        public static void DeregisterGob(ulong documentId, ulong instanceId, NativeObjectAdapter gob)
        {
            s_idToDomNode.Remove(new Pair<ulong, ulong>(documentId, instanceId));
        }

        // private static ulong s_currentDocumentId = 1;
        public static ulong CreateDocument(uint typeId)
        {
            // return s_currentDocumentId++; 
            return s_underlyingScene.CreateDocument(typeId, "");
        }

        // public static void ObjectAddChild(uint typeId, uint listId, ulong parentId, ulong childId)
        // {
        //     // a negative index means to 'append' the child to the parent list.
        //     NativeObjectAddChild(typeId, listId, parentId, childId, -1);
        // }
        // 
        // public static void ObjectInsertChild(uint typeId, uint listId, ulong parentId, ulong childId, int index)
        // {
        //     // a negative index means to 'append' the child to the parent list.
        //     NativeObjectAddChild(typeId, listId, parentId, childId, index);
        // }
        // 
        // public static void ObjectInsertChild(NativeObjectAdapter parent, NativeObjectAdapter child, uint listId,int index)
        // {
        //     uint typeId = parent != null ? parent.TypeId : 0;
        //     ulong parentId = parent != null ? parent.InstanceId : 0;
        //     ulong childId = child != null ? child.InstanceId : 0;
        //     ObjectInsertChild(typeId, listId, parentId, childId, index);
        // }
        // 
        // public static void ObjectAddChild(NativeObjectAdapter parent, NativeObjectAdapter child, uint listId)
        // {
        //     uint typeId = parent != null ? parent.TypeId : 0;
        //     ulong parentId = parent != null ? parent.InstanceId : 0;
        //     ulong childId = child != null ? child.InstanceId : 0;
        //     ObjectAddChild(typeId, listId, parentId, childId);
        // }
        // 
        // public static void ObjectRemoveChild(uint typeId, uint listId, ulong parentId, ulong childId)
        // {
        //     NativeObjectRemoveChild(typeId, listId, parentId, childId);
        // }
        // 
        // public static void ObjectRemoveChild(NativeObjectAdapter parent, NativeObjectAdapter child, uint listId)
        // {
        //     uint typeId = parent != null ? parent.TypeId : 0;
        //     ulong parentId = parent != null ? parent.InstanceId : 0;
        //     ulong childId = child != null ? child.InstanceId : 0;
        //     ObjectRemoveChild(typeId, listId, parentId, childId);
        // }

        public static void InvokeMemberFn(ulong instanceId, string fn, IntPtr arg, out IntPtr retVal)
        {
           NativeInvokeMemberFn(instanceId, fn, arg, out retVal);
        }

        public static void SetObjectProperty(uint typeid, ulong documentId, ulong instanceId, uint propId, Color color)
        {
            Vec4F val = new Vec4F(color.R/255.0f, color.G/255.0f, color.B/255.0f, color.A/255.0f);
            IntPtr ptr = new IntPtr(&val);
            int sz = Marshal.SizeOf(val);
            NativeSetObjectProperty(typeid, propId, documentId, instanceId, ptr, sz);
        }

        public static void SetObjectProperty(uint typeid, ulong documentId, ulong instanceId, uint propId, uint val)
        {
            IntPtr ptr = new IntPtr(&val);
            NativeSetObjectProperty(typeid, propId, documentId, instanceId, ptr, sizeof(uint));
        }

        public static void SetObjectProperty(uint typeid, ulong documentId, ulong instanceId, uint propId, Vec4F val)
        {
            IntPtr ptr = new IntPtr(&val);
            int sizeInBytes = Marshal.SizeOf(val);
            NativeSetObjectProperty(typeid, propId, documentId, instanceId, ptr, sizeInBytes);
        }

        public static void SetObjectProperty(uint typeid, ulong documentId, ulong instanceId, uint propId, Size sz)
        {
            IntPtr ptr = new IntPtr(&sz);
            int sizeInBytes = Marshal.SizeOf(sz);
            NativeSetObjectProperty(typeid, propId, documentId, instanceId, ptr, sizeInBytes);
        }

        public static void SetObjectProperty(uint typeid, ulong documentId, ulong instanceId, uint propId, IntPtr data, int size)
        {
            NativeSetObjectProperty(typeid, propId, documentId, instanceId, data, size);
        }

        public static void GetObjectProperty(uint typeId, uint propId, ulong instanceId, out int data)
        {
            int datasize = 0;
            IntPtr ptrData;
            GetObjectProperty(typeId, propId, instanceId, out ptrData, out datasize);
            if (datasize > 0)
            {
                data = *(int*)ptrData.ToPointer();
            }
            else
            {
                data = 0;
            }

        }

        public static void GetObjectProperty(uint typeId, uint propId, ulong instanceId, out uint data)
        {
            int datasize = 0;
            IntPtr ptrData;
            GetObjectProperty(typeId, propId, instanceId, out ptrData, out datasize);
            if (datasize > 0)
            {
                data = *(uint*)ptrData.ToPointer();
            }
            else
            {
                data = 0;
            }

        }
        public static void GetObjectProperty(uint typeId, uint propId, ulong instanceId, out IntPtr data, out int size)
        {
            NativeGetObjectProperty(typeId,propId,instanceId, out data,out size);
        }

        public static NativeObjectAdapter GetAdapterFromId(ulong documentId, ulong instanceId)
        {
            return s_idToDomNode[new Pair<ulong, ulong>(documentId, instanceId)];
        }

        #endregion

        #region picking and selection
        public static bool RayPick(Matrix4F viewxform, Matrix4F projxfrom, Ray3F rayW, bool skipSelected, out HitRecord hit) { hit = new HitRecord(); return false; }
        public static HitRecord[] RayPick(Matrix4F viewxform, Matrix4F projxfrom, Ray3F rayW, bool skipSelected) { return null; }
        public static void SetSelection(IEnumerable<NativeObjectAdapter> selection) { }

        // public static bool RayPick(Matrix4F viewxform, Matrix4F projxfrom, Ray3F rayW, bool skipSelected, out HitRecord hit)
        // {
        //     HitRecord* nativeHits = null;
        //     int count;
        // 
        //     //bool skipSelected,
        //     fixed (float* ptr1 = &viewxform.M11, ptr2 = &projxfrom.M11)
        //     {
        //         NativeRayPick(
        //         ptr1,
        //         ptr2,
        //         &rayW,
        //         skipSelected,
        //         &nativeHits,
        //         out count);
        //     }
        // 
        //     if(count > 0)
        //     {
        //         hit = *nativeHits;
        //     }
        //     else
        //     {
        //         hit = new HitRecord();
        //     }
        //    
        //     return count > 0;
        // 
        // }
        // public static HitRecord[] RayPick(Matrix4F viewxform, Matrix4F projxfrom, Ray3F rayW, bool skipSelected)
        // {
        //     HitRecord* nativeHits = null;
        //     int count;
        // 
        //     fixed (float* ptr1 = &viewxform.M11, ptr2 = &projxfrom.M11)
        //     {
        //         NativeRayPick(
        //         ptr1,
        //         ptr2,
        //         &rayW,
        //         skipSelected,
        //         &nativeHits,
        //         out count);                
        //     }
        //     
        //     var objects = new List<HitRecord>();
        // 
        //     for (int k = 0; k < count; k++)
        //     {                
        //         objects.Add(*nativeHits);                
        //         nativeHits++;
        //     }
        //     return objects.ToArray();
        // 
        // }
        // 
        // private static float[] s_rect = new float[4];
        // public static HitRecord[] FrustumPick(ulong renderSurface, Matrix4F viewxform,
        //                                        Matrix4F projxfrom,
        //                                        RectangleF rect)
        // {            
        //     s_rect[0] = rect.X;
        //     s_rect[1] = rect.Y;
        //     s_rect[2] = rect.Width;
        //     s_rect[3] = rect.Height;
        // 
        //     HitRecord* nativeHits = null;
        //     int count;
        // 
        //     fixed (float* ptr1 = &viewxform.M11, ptr2 = &projxfrom.M11)
        //     {
        //         NativeFrustumPick(
        //             renderSurface,
        //             ptr1,
        //             ptr2,
        //             s_rect,
        //             &nativeHits,
        //             out count);
        //     }
        // 
        //     var objects = new List<HitRecord>();
        //     
        //     for (int k = 0; k < count; k++)
        //     {
        //         HitRecord nativehit = *nativeHits;
        //         objects.Add(nativehit);                
        //         nativeHits++;
        //     }
        //     return objects.ToArray();
        // }
        // 
        // public static void SetSelection(IEnumerable<NativeObjectAdapter> selection)
        // {
        //     List<ulong> ids = new List<ulong>();
        //     foreach (NativeObjectAdapter nativeObj in selection)
        //     {
        //         ids.Add(nativeObj.InstanceId);
        //     }
        // 
        //     NativeSetSelection(ids.ToArray(), ids.Count);
        // }

        #endregion

        #region basic rendering 
        // create vertex buffer with given vertex format from user data.
        public static ulong CreateVertexBuffer(VertexPN[] buffer)
        {
            if (buffer == null || buffer.Length < 2)
                return 0;

            ulong vbId = 0;
            fixed (float* ptr = &buffer[0].Position.X)
            {
                vbId = NativeCreateVertexBuffer(VertexFormat.VF_PN, ptr, (uint)buffer.Length);
            }
            return vbId;
        }


        // create vertex buffer with given vertex format from user data.
        public static ulong CreateVertexBuffer(Vec3F[] buffer)
        {
            if (buffer == null || buffer.Length < 2)
                return 0;

            ulong vbId = 0;
            fixed (float* ptr = &buffer[0].X)
            {
                vbId = NativeCreateVertexBuffer(VertexFormat.VF_P, ptr, (uint)buffer.Length);
            }
            return vbId;
        }

        // Create index buffer from user data.
        public static ulong CreateIndexBuffer(uint[] buffer)
        {            
            fixed (uint* ptr = buffer)
            {
                return NativeCreateIndexBuffer(ptr, (uint)buffer.Length);
            }            
        }
    
        // deletes index/vertex buffer.
        public static void DeleteBuffer(ulong bufferId)
        {
            if (bufferId == 0) return;
            NativeDeleteBuffer(bufferId);
        }

        /// <summary>
        /// Sets render flags used for basic drawing.</summary>        
        public static void SetRendererFlag(BasicRendererFlags renderFlags)
        {
            NativeSetRendererFlag(renderFlags);
        }

        //Draw primitive with the given parameters.
        public static void DrawPrimitive(PrimitiveType pt,
                                            ulong vb,
                                            uint StartVertex,
                                            uint vertexCount,
                                            Color color,
                                            Matrix4F xform)
                                            
        {
            Vec4F vc;
            vc.X = color.R / 255.0f;
            vc.Y = color.G / 255.0f;
            vc.Z = color.B / 255.0f;
            vc.W = color.A / 255.0f;
            fixed (float* mtrx = &xform.M11)
            {
                NativeDrawPrimitive(pt, vb, StartVertex, vertexCount, &vc.X, mtrx);
            }

        }

        public static void DrawIndexedPrimitive(PrimitiveType pt,
                                                ulong vb,
                                                ulong ib,
                                                uint startIndex,
                                                uint indexCount,
                                                uint startVertex,
                                                Color color,
                                                Matrix4F xform)
                                                
        {
            Vec4F vc;
            vc.X = color.R / 255.0f;
            vc.Y = color.G / 255.0f;
            vc.Z = color.B / 255.0f;
            vc.W = color.A / 255.0f;
            fixed (float* mtrx = &xform.M11)
            {
                NativeDrawIndexedPrimitive(pt, vb, ib, startIndex, indexCount, startVertex, &vc.X, mtrx);
            }

        }
        #endregion

        #region font and text rendering

        public static ulong CreateFont(string fontName,float pixelHeight,FontStyle fontStyles) { return 0; }
        public static void DeleteFont(ulong fontId) {}
        public static void DrawText2D(string text, ulong fontId, int x, int y, Color color) {}

        #endregion

        #region private members

        private static uint NativeGetObjectTypeId(string className) { return s_underlyingScene.GetTypeId(className); }
        private static uint NativeGetDocumentTypeId(string className) { return s_underlyingScene.GetDocumentTypeId(className); }
        private static uint NativeGetObjectPropertyId(uint id, string propertyName) { return s_underlyingScene.GetPropertyId(id, propertyName); }
        private static uint NativeGetObjectChildListId(uint id, string listName) { return s_underlyingScene.GetChildListId(id, listName); }
        // private static ulong s_currentInstanceId = 1;
        private static ulong NativeCreateObject(ulong documentId, ulong existingId, uint typeId, IntPtr data, int size) 
        {
            // if (existingId!=0) return existingId;
            // return s_currentInstanceId++;
            if (existingId==0)
                existingId = s_underlyingScene.AssignObjectId(documentId, typeId);

            if (s_underlyingScene.CreateObject(documentId, existingId, typeId, ""))
                return existingId;
            return 0;
        }
        private static void NativeDestroyObject(ulong documentId, ulong instanceId, uint typeId) 
        {
            s_underlyingScene.DeleteObject(documentId, instanceId, typeId);
        }
        private static void NativeInvokeMemberFn(ulong instanceId, string fn, IntPtr arg, out IntPtr retVal) { retVal = IntPtr.Zero; }
        private static void NativeSetObjectProperty(uint typeId, uint propId, ulong documentId, ulong instanceId, IntPtr data, int size) 
        {
            unsafe
            {
                s_underlyingScene.SetProperty(documentId, instanceId, typeId, propId, data.ToPointer());
            }
        }

        static GCHandle s_savedBoundingBoxHandle;
        static bool s_builtSavedBoundingBox = false;
        private static void NativeGetObjectProperty(uint typeId, uint propId, ulong instanceId, out IntPtr data, out int size) 
        {
            if (!s_builtSavedBoundingBox) {
                Vec3F[] boundingBox = new Vec3F[2] { new Vec3F(-10.0f, -10.0f, -10.0f), new Vec3F(10.0f, 10.0f, 10.0f) };
                s_savedBoundingBoxHandle = GCHandle.Alloc(boundingBox, GCHandleType.Pinned);
                s_builtSavedBoundingBox = true;
            }
            data = s_savedBoundingBoxHandle.AddrOfPinnedObject();
            size = sizeof(float) * 6;
        }
        // private static void NativeObjectAddChild(uint typeid, uint listId, ulong parentId, ulong childId, int index) { }
        // private static void NativeObjectRemoveChild(uint typeid, uint listId, ulong parentId, ulong childId) { }

        private static ulong NativeCreateVertexBuffer(VertexFormat vf, void* buffer, uint vertexCount) 
        {
            return s_savedRenderResources.CreateVertexBuffer(
                buffer,
                vertexCount * vf.GetSize());
        }
        private static ulong NativeCreateIndexBuffer(uint* buffer, uint indexCount) 
        {
            return s_savedRenderResources.CreateIndexBuffer(buffer, 2*indexCount);
        }
        private static void NativeDeleteBuffer(ulong buffer) 
        {
            s_savedRenderResources.DeleteBuffer(buffer);
        }
        private static void NativeSetRendererFlag(BasicRendererFlags renderFlag) { }
        private static void NativeDrawPrimitive(PrimitiveType pt,
                                                        ulong vb,
                                                        uint StartVertex,
                                                        uint vertexCount,
                                                        float* color,
                                                        float* xform) { }
        private static void NativeDrawIndexedPrimitive(PrimitiveType pt,
                                                        ulong vb,
                                                        ulong ib,
                                                        uint startIndex,
                                                        uint indexCount,
                                                        uint startVertex,
                                                        float* color,
                                                        float* xform) { }

        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_Initialize", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeInitialize(LogCallbackType logCallback, InvalidateViewsDlg invalidateCallback,
        //     out IntPtr engineInfo);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_Shutdown", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeShutdown();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_Clear")]
        // private static extern void NativeClear();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_GetObjectTypeId", CallingConvention = CallingConvention.StdCall)]
        // private static extern uint NativeGetObjectTypeId(string className);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_GetObjectPropertyId", CallingConvention = CallingConvention.StdCall)]
        // private static extern uint NativeGetObjectPropertyId(uint id, string propertyName);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_GetObjectChildListId", CallingConvention = CallingConvention.StdCall)]
        // private static extern uint NativeGetObjectChildListId(uint id, string listName);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_CreateObject", CallingConvention = CallingConvention.StdCall)]
        // private static extern ulong NativeCreateObject(uint typeId, IntPtr data, int size);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_DestroyObject", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeDestroyObject(uint typeId, ulong instanceId);
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_InvokeMemberFn", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeInvokeMemberFn(ulong instanceId, string fn, IntPtr arg, out IntPtr retVal);
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SetObjectProperty", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeSetObjectProperty(uint typeId, uint propId, ulong instanceId, IntPtr data, int size);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_GetObjectProperty", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeGetObjectProperty(uint typeId, uint propId, ulong instanceId, out IntPtr data, out int size);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_ObjectAddChild", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeObjectAddChild(uint typeid, uint listId, ulong parentId, ulong childId, int index);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_ObjectRemoveChild", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeObjectRemoveChild(uint typeid, uint listId, ulong parentId, ulong childId);
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_RayPick", CallingConvention = CallingConvention.StdCall)]
        // private static extern bool NativeRayPick(
        //     [In] float* viewxform,
        //     [In] float* projxfrom,
        //     [In] Ray3F* rayW,
        //     [In] bool skipSelected,
        //     [Out] HitRecord** instanceIds,
        //     [Out] out int count);
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_FrustumPick", CallingConvention = CallingConvention.StdCall)]
        // private static extern bool NativeFrustumPick(
        //     [In]ulong renderSurface,
        //     [In]float* viewxform, 
        //     [In]float* projxfrom, 
        //     [In]float[] rect,
        //     [Out]HitRecord** instanceIds, 
        //     [Out]out int count);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SetSelection", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeSetSelection(ulong[] instanceIds, int count);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SetRenderState", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeSetRenderState(ulong instanceId);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SetGameLevel", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeSetGameLevel(ulong instanceId);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_GetGameLevel", CallingConvention = CallingConvention.StdCall)]
        // private static extern ulong NativeGetGameLevel();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_WaitForPendingResources", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeWaitForPendingResources();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_Update", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeUpdate(FrameTime* time, UpdateType updateType);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_Begin", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeBegin(ulong renderSurface, float* viewxform, float* projxfrom);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_End")]
        // private static extern void NativeEnd();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_RenderGame")]
        // private static extern void NativeRenderGame();
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SaveRenderSurfaceToFile", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        // private static extern bool NativeSaveRenderSurfaceToFile(ulong renderSurface, string fileName);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_CreateVertexBuffer", CallingConvention = CallingConvention.StdCall)]
        // private static extern ulong NativeCreateVertexBuffer(VertexFormat vf, void* buffer, uint vertexCount);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_CreateIndexBuffer", CallingConvention = CallingConvention.StdCall)]
        // private static extern ulong NativeCreateIndexBuffer(uint* buffer, uint indexCount);
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_DeleteBuffer", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeDeleteBuffer(ulong buffer);
        // 
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_SetRendererFlag", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeSetRendererFlag(BasicRendererFlags renderFlag);
        // 
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_DrawPrimitive", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeDrawPrimitive(PrimitiveType pt,
        //                                                 ulong vb,
        //                                                 uint StartVertex,
        //                                                 uint vertexCount,
        //                                                 float* color,
        //                                                 float* xform);
        //                                             
        // 
        // [DllImportAttribute("LvEdRenderingEngine", EntryPoint = "LvEd_DrawIndexedPrimitive", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeDrawIndexedPrimitive(PrimitiveType pt,
        //                                                 ulong vb,
        //                                                 ulong ib,
        //                                                 uint startIndex,
        //                                                 uint indexCount,
        //                                                 uint startVertex,
        //                                                 float* color,
        //                                                 float* xform);
        //                                                 
        // [DllImport("LvEdRenderingEngine", EntryPoint = "LvEd_CreateFont", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        // private static extern ulong NativeCreateFont(string fontName, float pixelHeight, FontStyle fontStyles);
        // 
        // [DllImport("LvEdRenderingEngine", EntryPoint = "LvEd_DeleteFont", CallingConvention = CallingConvention.StdCall)]
        // private static extern void NativeDeleteFont(ulong fontId);
        // 
        // [DllImport("LvEdRenderingEngine", EntryPoint = "LvEd_DrawText2D", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
        // private static extern void NativeDrawText2D(ulong fontId, string text, int x, int y, int color);
        
        private static string s_notInitialized = "Not initialized, please call Initialize()";
        
        private static void ThrowExceptionForLastError()
        {
            int hr = Marshal.GetHRForLastWin32Error();
            Marshal.ThrowExceptionForHR(hr);
        }

        //private static FntDlg GetFunction<FntDlg>(string fntName) where FntDlg : class
        //{
        //    IntPtr fntPtr = NativeMethods.GetProcAddress(s_libHandle, fntName);
        //    if (fntPtr == IntPtr.Zero)
        //        throw new ArgumentException("can't find functions: " + fntName);

        //    object dlg = Marshal.GetDelegateForFunctionPointer(fntPtr, typeof(FntDlg));
        //    return (FntDlg)dlg;
        //}

        private static Dictionary<Pair<ulong, ulong>, NativeObjectAdapter> s_idToDomNode = new Dictionary<Pair<ulong, ulong>, NativeObjectAdapter>();
        // private static class NativeMethods
        // {
        //     [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        //     public static extern IntPtr LoadLibrary(string fileName);
        // 
        //     [ReliabilityContract(Consistency.WillNotCorruptState, Cer.Success)]
        //     [DllImport("kernel32", SetLastError = true)]
        //     [return: MarshalAs(UnmanagedType.Bool)]
        //     public static extern bool FreeLibrary(IntPtr hModule);
        // 
        // 
        //     [DllImport("kernel32")]
        //     public static extern IntPtr GetProcAddress(IntPtr hModule, string procname);
        // 
        //     [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        //     public static extern IntPtr GetModuleHandle(string moduleName);
        // 
        // }



        //private class MessageFilter : IMessageFilter
        //{
        //    #region IMessageFilter Members

        //    bool IMessageFilter.PreFilterMessage(ref Message m)
        //    {
        //        const int WM_USER = 0x0400;
        //        const int InvalidateViews = WM_USER + 0x1;

        //        if (m.Msg == InvalidateViews)
        //        {
        //            RefreshView(this, EventArgs.Empty);
        //            return true;
        //        }
        //        return false;
        //    }

        //    #endregion

        //}
        #endregion
    }
}

