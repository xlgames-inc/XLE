// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Drawing;
using Sce.Atf;
using Sce.Atf.Applications;
using System;

namespace LevelEditorXLE.Terrain
{
    [Export(typeof(LevelEditorCore.IManipulator))]
    [Export(typeof(IInitializable))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TerrainManipulator : LevelEditorCore.IManipulator, IInitializable, XLEBridgeUtils.IShutdownWithEngine, IDisposable
    {
        public bool Pick(LevelEditorCore.ViewControl vc, Point scrPt)          { return _nativeManip.MouseMove(vc, scrPt); }
        public void Render(LevelEditorCore.ViewControl vc)                     { _nativeManip.Render(vc); }
        public void OnBeginDrag()                                              { _nativeManip.OnBeginDrag(); }
        public void OnDragging(LevelEditorCore.ViewControl vc, Point scrPt)    { _nativeManip.OnDragging(vc, scrPt); }
        public void OnEndDrag(LevelEditorCore.ViewControl vc, Point scrPt) 
		{
            _nativeManip.OnEndDrag(vc, scrPt);

			// we need to create operations and turn them into a transaction:
			// string transName = string.Format("Apply {0} brush", brush.Name);
			// 
			// GameContext context = m_designView.Context.As<GameContext>();
			// context.DoTransaction(
			// 	delegate
			// {
			// 	foreach(var op in m_tmpOps)
			// 		context.TransactionOperations.Add(op);
			// }, transName);
			// m_tmpOps.Clear();
		}

        public LevelEditorCore.ManipulatorInfo ManipulatorInfo
        {
            get
            {
                return new LevelEditorCore.ManipulatorInfo(
                    "Terrain".Localize(),
                    "Activate Terrain editing".Localize(),
                    LevelEditorCore.Resources.TerrainManipImage,
                    Keys.None);
            }
        }

        public XLEBridgeUtils.ActiveManipulatorContext ManipulatorContext
        {
            get { return _manipContext; }
        }

        public void Initialize()
        {
            _domChangeInspector = new XLEBridgeUtils.DomChangeInspector(m_contextRegistry);
            _domChangeInspector.OnActiveContextChanged += UpdateManipulatorContext;
            _manipContext.ManipulatorSet = XLEBridgeUtils.NativeManipulatorLayer.SceneManager.CreateTerrainManipulators();
            _nativeManip = new XLEBridgeUtils.NativeManipulatorLayer(_manipContext);
        }

        public void Shutdown()
        {
            if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
            if (_manipContext != null) { _manipContext.Dispose(); _nativeManip = null; }
        }

        TerrainManipulator()
        { 
            _manipContext = new XLEBridgeUtils.ActiveManipulatorContext(); 
            _nativeManip = null;
        }

        ~TerrainManipulator()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected void Dispose(bool disposing)
        {
            if (disposing) {
                if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
                if (_manipContext != null) { _manipContext.Dispose(); _nativeManip = null; }
            }
        }

        private void UpdateManipulatorContext(object obj)
        {
            var sceneMan = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            if (sceneMan != null) {
                _manipContext.ManipulatorSet = sceneMan.CreateTerrainManipulators();
            } else {
                _manipContext.ManipulatorSet = null;
            }
        }

        private XLEBridgeUtils.NativeManipulatorLayer _nativeManip;
        private XLEBridgeUtils.ActiveManipulatorContext _manipContext;
        private XLEBridgeUtils.DomChangeInspector _domChangeInspector;
        [Import(AllowDefault = false)] IContextRegistry m_contextRegistry;
    };
}


