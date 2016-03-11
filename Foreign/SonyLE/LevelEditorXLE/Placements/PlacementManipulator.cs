// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Drawing;
using Sce.Atf;
using Sce.Atf.Applications;
using LevelEditorXLE.Extensions;
using LevelEditorCore;

#pragma warning disable 0649 // Field '...' is never assigned to, and will always have its default value null

namespace LevelEditorXLE.Placements
{
    [Export(typeof(LevelEditorCore.IManipulator))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PlacementManipulator : IManipulator, XLEBridgeUtils.IShutdownWithEngine, IDisposable
    {
        public ManipulatorPickResult Pick(ViewControl vc, Point scrPt)
        {
            if (_nativeManip.MouseMove(vc as GUILayer.IViewContext, scrPt))
                return ManipulatorPickResult.ImmediateBeginDrag;
            return ManipulatorPickResult.Miss;
        }
        public void OnBeginDrag(ViewControl vc, Point scrPt)   { _nativeManip.OnBeginDrag(vc as GUILayer.IViewContext, scrPt); }
        public void OnDragging(ViewControl vc, Point scrPt)    { _nativeManip.OnDragging(vc as GUILayer.IViewContext, scrPt); }
        public void OnEndDrag(ViewControl vc, Point scrPt)     { _nativeManip.OnEndDrag(vc as GUILayer.IViewContext, scrPt); }
        public void OnMouseWheel(ViewControl vc, Point scrPt, int delta) { _nativeManip.OnMouseWheel(vc as GUILayer.IViewContext, scrPt, delta); }

        public void Render(object opaqueContext, ViewControl vc) 
        {
            var context = opaqueContext as GUILayer.SimpleRenderingContext;
            if (context == null) return;
            _nativeManip.Render(context); 
        }

        public ManipulatorInfo ManipulatorInfo
        {
            get
            {
                return new ManipulatorInfo(
                    "Placements".Localize(),
                    "Activate placements editing".Localize(),
                    LevelEditorCore.Resources.TerrainManipImage,
                    Keys.None);
            }
        }

        public void Shutdown()
        {
            var r = _contextRegistry.Target as IContextRegistry;
            if (r != null) r.ActiveContextChanged -= OnActiveContextChanged;

            var rl = _resourceLister.Target as LevelEditorCore.ResourceLister;
            if (rl != null) rl.SelectionChanged -= resourceLister_SelectionChanged;

            if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
            if (_manipContext != null) { _manipContext.Dispose(); _manipContext = null; }
        }

        [ImportingConstructor]
        public PlacementManipulator(IContextRegistry contextRegistry, LevelEditorCore.ResourceLister resourceLister)
        {
            _manipContext = new GUILayer.ActiveManipulatorContext();
            _nativeManip = null;

            _manipSettings = new Settings();
            _nativeManip = new GUILayer.NativeManipulatorLayer(_manipContext);

            if (resourceLister != null)
            {
                resourceLister.SelectionChanged += resourceLister_SelectionChanged;
                _resourceLister = new WeakReference(resourceLister);
            }

            _contextRegistry = new WeakReference(contextRegistry);
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
            _nativeManip = new GUILayer.NativeManipulatorLayer(_manipContext);
        }

        public void Dispose() { Shutdown(); }

        private class Settings : GUILayer.IPlacementManipulatorSettingsLayer
        {
            public override string GetSelectedModel() { return _selectedModel; }
            public override string GetSelectedMaterial() { return _selectedMaterial; }
            public override void SelectModel(string newModelName, string materialName) { _selectedModel = newModelName; _selectedMaterial = materialName; }
            public override void EnableSelectedModelDisplay(bool newState) {}
            public override void SwitchToMode(uint newMode) {}

            internal Settings()
            {
                _selectedModel = "game/model/galleon/galleon";
                _selectedMaterial = "game/model/galleon/galleon";
            }

            internal String _selectedModel;
            internal String _selectedMaterial;
        };

        private void OnActiveContextChanged(object obj, EventArgs args)
        {
            GUILayer.EditorSceneManager sceneMan = null;

            IContextRegistry registry = obj as IContextRegistry;
            if (registry != null)
            {
                var gameExt = registry.GetActiveContext<Game.GameExtensions>();
                if (gameExt != null)
                    sceneMan = gameExt.SceneManager;
            }

            if (sceneMan != null)
            {
                _manipContext.ManipulatorSet = sceneMan.CreatePlacementManipulators(_manipSettings);
            }
            else
            {
                _manipContext.ManipulatorSet = null;
            }
        }

        public System.Windows.Forms.Control GetHoveringControl() { return null; }
        public event System.EventHandler OnHoveringControlChanged;

        private GUILayer.NativeManipulatorLayer _nativeManip;
        private GUILayer.ActiveManipulatorContext _manipContext;
        private Settings _manipSettings;

        [Import(AllowDefault = false)] IXLEAssetService _assetService;
        WeakReference _contextRegistry;
        WeakReference _resourceLister;

        void resourceLister_SelectionChanged(object sender, EventArgs e)
        {
            var resLister = sender as LevelEditorCore.ResourceLister;
            var resourceUri = resLister.LastSelected;
            if (resourceUri != null) {
                _manipSettings._selectedModel = 
                    _assetService.StripExtension(_assetService.AsAssetName(resourceUri));
                _manipSettings._selectedMaterial = _manipSettings._selectedModel;
            }
        }
    };
}

