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

namespace LevelEditorXLE.Placements
{
    [Export(typeof(LevelEditorCore.IManipulator))]
    [Export(typeof(IInitializable))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PlacementManipulator : LevelEditorCore.IManipulator, IInitializable, XLEBridgeUtils.IShutdownWithEngine, IDisposable
    {
        public bool Pick(LevelEditorCore.ViewControl vc, Point scrPt)          { return _nativeManip.MouseMove(vc, scrPt); }
        public void Render(LevelEditorCore.ViewControl vc)                     { _nativeManip.Render(vc); }
        public void OnBeginDrag()                                              { _nativeManip.OnBeginDrag(); }
        public void OnDragging(LevelEditorCore.ViewControl vc, Point scrPt)    { _nativeManip.OnDragging(vc, scrPt); }
        public void OnEndDrag(LevelEditorCore.ViewControl vc, Point scrPt)     { _nativeManip.OnEndDrag(vc, scrPt); }
        public void OnMouseWheel(LevelEditorCore.ViewControl vc, Point scrPt, int delta) { _nativeManip.OnMouseWheel(vc, scrPt, delta); }

        public LevelEditorCore.ManipulatorInfo ManipulatorInfo
        {
            get
            {
                return new LevelEditorCore.ManipulatorInfo(
                    "Placements".Localize(),
                    "Activate placements editing".Localize(),
                    LevelEditorCore.Resources.TerrainManipImage,
                    Keys.None);
            }
        }

        public void Initialize()
        {
            _manipSettings = new Settings();
            _manipContext.ManipulatorSet = XLEBridgeUtils.NativeManipulatorLayer.SceneManager.CreatePlacementManipulators(_manipSettings);
            // _controls->ActiveContext = _manipContext;
            _nativeManip = new XLEBridgeUtils.NativeManipulatorLayer(_manipContext);

            if (_resourceLister != null)
                _resourceLister.SelectionChanged += resourceLister_SelectionChanged;
        }

        public void Shutdown()
        {
            if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
            if (_manipContext != null) { _manipContext.Dispose(); _manipContext = null; }
        }

        public PlacementManipulator()
        {
            _manipContext = new XLEBridgeUtils.ActiveManipulatorContext();
            _nativeManip = null;
        }

        ~PlacementManipulator()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing) {
                if (_nativeManip != null) { _nativeManip.Dispose(); _nativeManip = null; }
                if (_manipContext != null) { _manipContext.Dispose(); _manipContext = null; }
            }
        }

        private class Settings : GUILayer.IPlacementManipulatorSettingsLayer
        {
            public override string GetSelectedModel() { return _selectedModel; }
            public override string GetSelectedMaterial() { return _selectedMaterial; }
            public override void SelectModel(string newModelName, string materialName) { _selectedModel = newModelName; _selectedMaterial = materialName; }
            public override void EnableSelectedModelDisplay(bool newState) {}
            public override void SwitchToMode(uint newMode) {}

            internal Settings()
            {
                _selectedModel = "game/model/nature/bushtree/BushE";
                _selectedMaterial = "game/model/nature/bushtree/BushE";
            }

            internal String _selectedModel;
            internal String _selectedMaterial;
        };

        XLEBridgeUtils.NativeManipulatorLayer _nativeManip;
        XLEBridgeUtils.ActiveManipulatorContext _manipContext;
        Settings _manipSettings;

        [Import(AllowDefault =  true)] LevelEditorCore.ResourceLister _resourceLister;
        [Import(AllowDefault = false)] IXLEAssetService _assetService;

        void resourceLister_SelectionChanged(object sender, EventArgs e)
        {
            var resourceUri = _resourceLister.LastSelected;
            if (resourceUri != null) {
                _manipSettings._selectedModel = 
                    _assetService.StripExtension(_assetService.AsAssetName(resourceUri));
                _manipSettings._selectedMaterial = _manipSettings._selectedModel;
            }
        }
    };
}

