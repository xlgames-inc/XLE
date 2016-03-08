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
    using PropertyInitializer = GUILayer.EntityLayer.PropertyInitializer;

    /// <summary>
    /// Exposes a minimum set of game-engine functionalities 
    /// for LevelEditor purpose.</summary>    
    [Export(typeof(IGameEngineProxy))]
    [Export(typeof(INativeIdMapping))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    [SuppressUnmanagedCodeSecurity()]
    public unsafe class GameEngine : DisposableObject, IGameEngineProxy, INativeIdMapping
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

        public void Update(FrameTime ft, UpdateType updateType)
        {
                // There are 3 update types defined by Sony.
                //  Here they are:
            // Editing: in this mode physics and AI 
            // should not be updated.
            // While particle system and other editing related effects
            // should be updated
    
            // GamePlay: update all the subsystems.
            // Some editing related effects shoult not updated.

            // Paused: none of the time based effects are simulated.
            // Delta time should be zero.

            if (updateType != UpdateType.Paused)
                s_underlyingScene.IncrementTime(ft.ElapsedTime);
        }

        public void SetGameWorld(IGame game) {}
        public void WaitForPendingResources() {}
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
        private static GUILayer.RetainedRenderResources s_retainedRenderResources;
        private static GUILayer.EditorSceneManager s_underlyingScene;
        private static GUILayer.EntityLayer s_entityInterface;
        private static XLEBridgeUtils.LoggingRedirect s_loggingRedirect;

        private static System.Windows.Forms.Timer s_foregroundUpdateTimer;

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
                s_retainedRenderResources = new GUILayer.RetainedRenderResources(s_engineDevice);
                s_underlyingScene = new GUILayer.EditorSceneManager();
                Util3D.Init();
                XLEBridgeUtils.Utils.GlobalSceneManager = s_underlyingScene;
                s_entityInterface = s_underlyingScene.GetEntityInterface();
                CriticalError = "";
                s_inist.PopulateEngineInfo(
                    @"<EngineInfo>
                        <SupportedResources>
                            <ResourceDescriptor Type='Model' Name='Model' Description='Model' Ext='.dae' />
                            <ResourceDescriptor Type='ModelBookmark' Name='ModelBookmark' Description='ModelBookmark' Ext='.modelbookmark' />
                            <ResourceDescriptor Type='Texture' Name='Texture' Description='Texture' Ext='.dds,.tga,.bmp,.jpg,.jpeg,.png,.tif,.tiff,.gif,.hpd,.jxr,.wdp,.ico,.hdr,.exr' />
                            <ResourceDescriptor Type='Prefab' Name='Prefab' Description='Prefab' Ext='.prefab' />
                        </SupportedResources>
                    </EngineInfo>");

                XLEBridgeUtils.Utils.AttachLibrary(s_engineDevice);
                s_loggingRedirect = new XLEBridgeUtils.LoggingRedirect();

                s_foregroundUpdateTimer = new System.Windows.Forms.Timer();
                s_foregroundUpdateTimer.Tick += s_foregroundUpdateTimer_Elapsed;
                s_foregroundUpdateTimer.Interval = 32;
                s_foregroundUpdateTimer.Start();
            }
            catch (Exception e)
            {
                CriticalError = "Error while initialising engine device: " + e.Message;
            }
        }

        static void s_foregroundUpdateTimer_Elapsed(object myObject, EventArgs myEventArgs)
        {
            if (s_engineDevice!=null)
                s_engineDevice.ForegroundUpdate();
        }

        public static bool IsInError { get { return CriticalError.Length > 0; } }
        public static string CriticalError { get; private set; }

        public static GUILayer.EditorSceneManager GetEditorSceneManager() { return s_underlyingScene; }
        public static GUILayer.EngineDevice GetEngineDevice() { return s_engineDevice; }
        public static GUILayer.RetainedRenderResources GetSavedResources() { return s_retainedRenderResources; }

        /// <summary>
        /// shutdown game engine.
        /// call it one time on application exit.
        /// </summary>
        public static void Shutdown()
        {
            foreach (var initializable in Globals.MEFContainer.GetExportedValues<XLEBridgeUtils.IShutdownWithEngine>())
                initializable.Shutdown();

            foreach (var keyValue in s_idToDomNode)
            {
                DestroyObject(
                    keyValue.Key.Item1,
                    keyValue.Key.Item2, 
                    keyValue.Value.TypeId);
            }
            s_idToDomNode.Clear();
            s_foregroundUpdateTimer.Stop();
            s_foregroundUpdateTimer.Dispose();
            s_foregroundUpdateTimer = null;

            s_loggingRedirect.Dispose();
            s_loggingRedirect = null;
            XLEBridgeUtils.Utils.DetachLibrary();
            Util3D.Shutdown();
            XLEBridgeUtils.Utils.GlobalSceneManager = null;
            s_entityInterface = null;
            s_underlyingScene.Dispose();
            s_underlyingScene = null;
            s_retainedRenderResources.Dispose();
            s_retainedRenderResources = null;
            s_engineDevice.Dispose();
            s_engineDevice = null;
            GlobalSelection.Dispose();
            GlobalSelection = null;
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

        // #region log callbacks
        // 
        // [UnmanagedFunctionPointer(CallingConvention.StdCall,CharSet = CharSet.Unicode)]
        // private delegate void LogCallbackType(int messageType, string text);
        // private static void LogCallback(int messageType, string text)
        // {
        //     Console.Write(text);
        //     if (messageType == (int)OutputMessageType.Warning
        //         || messageType == (int)OutputMessageType.Error)
        //         Outputs.Write((OutputMessageType)messageType, text);                            
        // }
        // 
        // #endregion

        #region Object management.

        public static uint GetObjectTypeId(string className)
        {
            if (IsInError) return 0;
            uint id = s_entityInterface.GetTypeId(className);
            if (id == 0)
            {
                System.Diagnostics.Debug.WriteLine(className + " is not defined in runtime");
            }
            return id;
        }

        public static uint GetDocumentTypeId(string className)
        {
            if (IsInError) return 0;
            uint id = s_entityInterface.GetDocumentTypeId(className);
            if (id == 0)
            {
                System.Diagnostics.Debug.WriteLine(className + " document type is not defined in runtime");
            }
            return id;
        }

        public static uint GetObjectPropertyId(uint typeId, string propertyName)
        {
            if (IsInError) return 0;
            uint propId = s_entityInterface.GetPropertyId(typeId, propertyName);
            if (propId == 0)
            {
                System.Diagnostics.Debug.WriteLine(propertyName + " is not defined for typeid " + typeId);
            }
            return propId;
        }

        public static uint GetObjectChildListId(uint typeId, string listName)
        {
            if (IsInError) return 0;
            uint propId = s_entityInterface.GetChildListId(typeId, listName);
            if (propId == 0)
            {
                System.Diagnostics.Debug.WriteLine(listName + " is not defined for typeid " + typeId);
            }
            return propId;
        }

        public static PropertyInitializer CreateInitializer(uint prop, void* ptr, Type elementType, uint arrayCount, bool isString)
        {
            return new PropertyInitializer(
                prop, ptr, 
                GUILayer.EditorInterfaceUtils.AsTypeId(elementType), arrayCount, isString);
        }

        public static ulong CreateObject(
            ulong documentId, ulong existingId, uint typeId,
            IEnumerable<PropertyInitializer> initializers)
        {
            if (existingId == 0)
                existingId = s_entityInterface.AssignObjectId(documentId, typeId);

            if (s_entityInterface.CreateObject(documentId, existingId, typeId, initializers))
                return existingId;
            return 0;
        }

        public static void DestroyObject(ulong documentId, ulong instanceId, uint typeId)
        {
            s_entityInterface.DeleteObject(documentId, instanceId, typeId);
        }

        public static void SetTypeAnnotation(
            uint typeId, string annotationName, IEnumerable<PropertyInitializer> initializers)
        {
            s_underlyingScene.SetTypeAnnotation(typeId, annotationName, initializers);
        }

        public static void RegisterGob(ulong documentId, ulong instanceId, NativeObjectAdapter gob)
        {
            s_idToDomNode.Add(Tuple.Create(documentId, instanceId), gob);
        }

        public static void DeregisterGob(ulong documentId, ulong instanceId, NativeObjectAdapter gob)
        {
            s_idToDomNode.Remove(Tuple.Create(documentId, instanceId));
        }

        public static ulong CreateDocument(uint docTypeId)
        {
            return s_entityInterface.CreateDocument(docTypeId);
        }

        public static void DeleteDocument(ulong docId, uint docTypeId)
        {
            s_entityInterface.DeleteDocument(docId, docTypeId);
        }

        public static void InvokeMemberFn(ulong instanceId, string fn, IntPtr arg, out IntPtr retVal)
        {
            throw new NotImplementedException();
        }

        public static void SetObjectProperty(
            uint typeId, ulong documentId, ulong instanceId,
            IEnumerable<PropertyInitializer> initializers)
        {
            unsafe
            {
                s_entityInterface.SetProperty(
                    documentId, instanceId, typeId, initializers);
            }
        }

        public static void SetObjectParent(
            ulong documentId, ulong childInstanceId, uint childTypeId,
            ulong parentInstanceId, uint parentTypeId,
            int insertionPosition)
        {
            s_entityInterface.SetObjectParent(documentId, 
                childInstanceId, childTypeId,
                parentInstanceId, parentTypeId, insertionPosition);
        }

        private static GCHandle s_savedBoundingBoxHandle;
        private static GCHandle s_temporaryNativeBuffer;
        private static uint s_temporaryNativeBufferSize = 0;
        private static bool s_builtSavedBoundingBox = false;

        public static void GetObjectProperty(uint typeId, uint propId, ulong documentId, ulong instanceId, out IntPtr data, out int size)
        {
            if (!s_builtSavedBoundingBox)
            {

                Vec3F[] boundingBox = new Vec3F[2] { new Vec3F(-10.0f, -10.0f, -10.0f), new Vec3F(10.0f, 10.0f, 10.0f) };
                s_savedBoundingBoxHandle = GCHandle.Alloc(boundingBox, GCHandleType.Pinned);
                s_builtSavedBoundingBox = true;

                var bufferInit = new uint[64];
                s_temporaryNativeBuffer = GCHandle.Alloc(bufferInit, GCHandleType.Pinned);
                s_temporaryNativeBufferSize = (uint)sizeof(uint) * 64;
            }

            unsafe
            {
                uint bufferSize = s_temporaryNativeBufferSize;
                IntPtr pinnedPtr = s_temporaryNativeBuffer.AddrOfPinnedObject();
                if (s_entityInterface.GetProperty(
                    documentId, instanceId, typeId, propId,
                    pinnedPtr.ToPointer(), &bufferSize))
                {
                    data = s_temporaryNativeBuffer.AddrOfPinnedObject();
                    size = (int)bufferSize;
                    return;
                }
            }

            data = s_savedBoundingBoxHandle.AddrOfPinnedObject();
            size = sizeof(float) * 6;
        }

        public IAdaptable GetAdapter(ulong nativeDocId, ulong nativeObjectId)
        {
            NativeObjectAdapter result;
            if (s_idToDomNode.TryGetValue(Tuple.Create(nativeDocId, nativeObjectId), out result))
                return result;
            return null;
        }

        #endregion

        public static GUILayer.ObjectSet GlobalSelection = new GUILayer.ObjectSet();

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

        public static ulong CreateVertexBuffer(VertexPC[] buffer)
        {
            if (buffer == null || buffer.Length < 2)
                return 0;

            ulong vbId = 0;
            fixed (float* ptr = &buffer[0].Position.X)
            {
                vbId = NativeCreateVertexBuffer(VertexFormat.VF_PC, ptr, (uint)buffer.Length);
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
        public static void SetRendererFlag(GUILayer.SimpleRenderingContext context, BasicRendererFlags renderFlags)
        {
            context.InitState(
                (renderFlags & BasicRendererFlags.DisableDepthTest) == 0,
                (renderFlags & BasicRendererFlags.DisableDepthWrite) == 0);
        }

        //Draw primitive with the given parameters.
        public static void DrawPrimitive(
            GUILayer.SimpleRenderingContext context,
            PrimitiveType pt,
            ulong vb, uint StartVertex, uint vertexCount,
            Color color, Matrix4F xform)
        {
            Vec4F vc;
            vc.X = color.R / 255.0f;
            vc.Y = color.G / 255.0f;
            vc.Z = color.B / 255.0f;
            vc.W = color.A / 255.0f;
            fixed (float* mtrx = &xform.M11)
            {
                NativeDrawPrimitive(context, pt, vb, StartVertex, vertexCount, &vc.X, mtrx);
            }

        }

        public static void DrawIndexedPrimitive(
            GUILayer.SimpleRenderingContext context, 
            PrimitiveType pt,
            ulong vb, ulong ib,
            uint startIndex, uint indexCount, uint startVertex,
            Color color, Matrix4F xform)
        {
            Vec4F vc;
            vc.X = color.R / 255.0f;
            vc.Y = color.G / 255.0f;
            vc.Z = color.B / 255.0f;
            vc.W = color.A / 255.0f;
            fixed (float* mtrx = &xform.M11)
            {
                NativeDrawIndexedPrimitive(context, pt, vb, ib, startIndex, indexCount, startVertex, &vc.X, mtrx);
            }
        }
        #endregion

        #region font and text rendering

        public static ulong CreateFont(string fontName,float pixelHeight,FontStyle fontStyles) { return 0; }
        public static void DeleteFont(ulong fontId) {}
        public static void DrawText2D(string text, ulong fontId, int x, int y, Color color) {}

        #endregion

        #region private members

        private static ulong NativeCreateVertexBuffer(VertexFormat vf, void* buffer, uint vertexCount) 
        {
            return s_retainedRenderResources.CreateVertexBuffer(
                buffer,
                vertexCount * vf.GetSize(),
                (uint)vf);
        }
        private static ulong NativeCreateIndexBuffer(uint* buffer, uint indexCount) 
        {
            return s_retainedRenderResources.CreateIndexBuffer(buffer, sizeof(uint)*indexCount);
        }
        private static void NativeDeleteBuffer(ulong buffer) 
        {
            s_retainedRenderResources.DeleteBuffer(buffer);
        }
        private static void NativeDrawPrimitive(
            GUILayer.SimpleRenderingContext context,
            PrimitiveType pt,
            ulong vb, uint StartVertex, uint vertexCount,
            float* color, float* xform) 
        {
            context.DrawPrimitive((uint)pt, vb, StartVertex, vertexCount, color, xform);
        }

        private static void NativeDrawIndexedPrimitive(
            GUILayer.SimpleRenderingContext context, 
            PrimitiveType pt,
            ulong vb, ulong ib,
            uint startIndex, uint indexCount, uint startVertex,
            float* color, float* xform) 
        {
            context.DrawIndexedPrimitive((uint)pt, vb, ib, startIndex, indexCount, startVertex, color, xform);
        }

        private static string s_notInitialized = "Not initialized, please call Initialize()";
        
        private static void ThrowExceptionForLastError()
        {
            int hr = Marshal.GetHRForLastWin32Error();
            Marshal.ThrowExceptionForHR(hr);
        }

        private static Dictionary<Tuple<ulong, ulong>, NativeObjectAdapter> s_idToDomNode 
            = new Dictionary<Tuple<ulong, ulong>, NativeObjectAdapter>();
        #endregion
    }
}

