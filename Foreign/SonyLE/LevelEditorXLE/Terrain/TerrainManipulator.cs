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
using LevelEditorXLE.Extensions;

namespace LevelEditorXLE.Terrain
{
    [Export(typeof(LevelEditorCore.IManipulator))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TerrainManipulator : LevelEditorCore.IManipulator, XLEBridgeUtils.IShutdownWithEngine, IDisposable, XLEBridgeUtils.IManipulatorExtra
    {
        public bool Pick(LevelEditorCore.ViewControl vc, Point scrPt)          
        {
            try
            {
                return _nativeManip.MouseMove(vc as GUILayer.IViewContext, scrPt);
            }
            catch (Exception e)
            {
                Outputs.Write(OutputMessageType.Warning, "Suppressing error in TerrainManipulator: " + e.Message);
                return false;
            }
        }
        public void OnBeginDrag()                                              
        { 
            try
            {
                _nativeManip.OnBeginDrag();
            }
            catch (Exception e)
            {
                Outputs.Write(OutputMessageType.Warning, "Suppressing error in TerrainManipulator: " + e.Message);
            }
        }
        public void OnDragging(LevelEditorCore.ViewControl vc, Point scrPt)    
        {
            try
            {
                _nativeManip.OnDragging(vc as GUILayer.IViewContext, scrPt);
            }
            catch (Exception e)
            {
                // we want to report this error as a hover message above "srcPt" in the view control
                Point msgPt = scrPt;
                msgPt.X += 20;
                msgPt.Y -= 20;
                vc.ShowHoverMessage(e.Message, msgPt);
            }
        }

        public void OnEndDrag(LevelEditorCore.ViewControl vc, Point scrPt) 
		{
            try
            {
                _nativeManip.OnEndDrag(vc as GUILayer.IViewContext, scrPt);
            }
            catch (Exception e)
            {
                // we want to report this error as a hover message above "srcPt" in the view control
                Point msgPt = scrPt;
                msgPt.X += 20;
                msgPt.Y -= 20;
                vc.ShowHoverMessage(e.Message, msgPt);
            }

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
        public void OnMouseWheel(LevelEditorCore.ViewControl vc, Point scrPt, int delta) 
        {
            try
            {
                _nativeManip.OnMouseWheel(vc as GUILayer.IViewContext, scrPt, delta);
                // Scrolling the mouse wheel will often change the properties... We need to raise
                // an event to make sure the new property values are reflected in the interface.
                _manipContext.RaisePropertyChange();
            }
            catch (Exception e)
            {
                Outputs.Write(OutputMessageType.Warning, "Suppressing error in TerrainManipulator: " + e.Message);
            }
        }

        public void Render(object opaqueContext, LevelEditorCore.ViewControl vc) 
        {
            var context = opaqueContext as GUILayer.SimpleRenderingContext;
            if (context == null) return;
            _nativeManip.Render(context); 
        }

        public bool ClearBeforeDraw() { return false; }

        public LevelEditorCore.ManipulatorInfo ManipulatorInfo
        {
            get
            {
                return new LevelEditorCore.ManipulatorInfo(
                    "Terrain".Localize(),
                    "Activate Terrain editing".Localize(),
                    Resources.TerrainManip,
                    Keys.None);
            }
        }

        public GUILayer.ActiveManipulatorContext ManipulatorContext
        {
            get { return _manipContext; }
        }

        public void Shutdown()
        {
            var r = _contextRegistry.Target as IContextRegistry;
            if (r != null) r.ActiveContextChanged -= OnActiveContextChanged;
            if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
            if (_manipContext != null) { _manipContext.Dispose(); _manipContext = null; }
        }

        [ImportingConstructor]
        public TerrainManipulator(IContextRegistry contextRegistry)
        {
            _manipContext = new GUILayer.ActiveManipulatorContext(); 
            _nativeManip = null;

            _contextRegistry = new WeakReference(contextRegistry);
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
            _nativeManip = new GUILayer.NativeManipulatorLayer(_manipContext);
        }

        public void Dispose() { Shutdown(); }

        private void OnActiveContextChanged(object obj, EventArgs args)
        {
            GUILayer.EditorSceneManager sceneMan = null;
            GUILayer.TerrainManipulatorContext maniContext = null;
            
            IContextRegistry registry = obj as IContextRegistry;
            if (registry != null)
            {
                var gameExt = registry.GetActiveContext<Game.GameExtensions>();
                if (gameExt != null)
                {
                    sceneMan = gameExt.SceneManager;
                    maniContext = gameExt.TerrainManipulatorContext;
                }
            }

            if (sceneMan != null && maniContext != null) {
                _manipContext.ManipulatorSet = sceneMan.CreateTerrainManipulators(maniContext);
            } else {
                _manipContext.ManipulatorSet = null;
            }
        }

        private GUILayer.NativeManipulatorLayer _nativeManip;
        private GUILayer.ActiveManipulatorContext _manipContext;
        private WeakReference _contextRegistry;
    };
}


