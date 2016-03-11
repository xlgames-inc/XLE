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
using LevelEditorCore;

namespace LevelEditorXLE.Terrain
{
    [Export(typeof(IManipulator))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TerrainManipulator : IManipulator, XLEBridgeUtils.IShutdownWithEngine, IDisposable, XLEBridgeUtils.IManipulatorExtra
    {
        public ManipulatorPickResult Pick(ViewControl vc, Point scrPt)
        {
            try
            {
                if (_nativeManip.OnHover(vc as GUILayer.IViewContext, scrPt))
                    return ManipulatorPickResult.ImmediateBeginDrag;
                return ManipulatorPickResult.Miss;
            }
            catch (Exception e)
            {
                Outputs.Write(OutputMessageType.Warning, "Suppressing error in TerrainManipulator: " + e.Message);
                return ManipulatorPickResult.Miss;
            }
        }

        public void OnBeginDrag(ViewControl vc, Point scrPt)
        { 
            try
            {
                _nativeManip.OnBeginDrag(vc as GUILayer.IViewContext, scrPt);
            }
            catch (Exception e)
            {
                Outputs.Write(OutputMessageType.Warning, "Suppressing error in TerrainManipulator: " + e.Message);
            }
        }

        public void OnDragging(ViewControl vc, Point scrPt)
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

        public void OnEndDrag(ViewControl vc, Point scrPt)
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
        }

        public void OnMouseWheel(ViewControl vc, Point scrPt, int delta)
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

        public void Render(object opaqueContext, ViewControl vc)
        {
            var context = opaqueContext as GUILayer.SimpleRenderingContext;
            if (context == null) return;
            _nativeManip.Render(context); 
        }

        public bool ClearBeforeDraw() { return false; }

        public Control GetHoveringControl()
        {
            if (_manipContext.ActiveManipulator == "Paint Coverage")
            {
                var ctrl = new System.Windows.Forms.ComboBox();
                ctrl.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
                return ctrl;
            }
            return null;
        }
        public event System.EventHandler OnHoveringControlChanged;

        public ManipulatorInfo ManipulatorInfo
        {
            get
            {
                return new ManipulatorInfo(
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
            _manipContext.OnActiveManipulatorChange += 
                (object sender, EventArgs e) =>
                {
                    if (this.OnHoveringControlChanged != null)
                        this.OnHoveringControlChanged(null, EventArgs.Empty);
                };
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


