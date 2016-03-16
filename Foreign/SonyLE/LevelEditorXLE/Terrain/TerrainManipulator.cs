// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Drawing;
using System.Collections.Generic;
using System;
using Sce.Atf;
using Sce.Atf.Applications;
using LevelEditorXLE.Extensions;
using LevelEditorCore;

namespace LevelEditorXLE.Terrain
{
    [Export(typeof(TerrainNamingBridge))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TerrainNamingBridge
    {
        public IEnumerable<Tuple<string, int>> BaseTextureMaterials { get { return _baseTextureMaterials; } }
        public IEnumerable<Tuple<string, int>> DecorationMaterials { get { return _decorationMaterials; } }

        public event EventHandler<EventArgs> OnBaseTextureMaterialsChanged;
        public event EventHandler<EventArgs> OnDecorationMaterialsChanged;

        public void SetBaseTextureMaterials(IEnumerable<Tuple<string, int>> range)
        {
            _baseTextureMaterials.Clear();
            _baseTextureMaterials.AddRange(range);
            if (OnBaseTextureMaterialsChanged != null)
                OnBaseTextureMaterialsChanged(this, EventArgs.Empty);
        }

        public void SetDecorationMaterials(IEnumerable<Tuple<string, int>> range)
        {
            _decorationMaterials.Clear();
            _decorationMaterials.AddRange(range);
            if (OnDecorationMaterialsChanged != null)
                OnDecorationMaterialsChanged(this, EventArgs.Empty);
        }

        public TerrainNamingBridge()
        {
            _baseTextureMaterials = new List<Tuple<string, int>>();
            _decorationMaterials = new List<Tuple<string, int>>();
        }

        private List<Tuple<string, int>> _baseTextureMaterials;
        private List<Tuple<string, int>> _decorationMaterials;
    }

    [Export(typeof(IManipulator))]
    [Export(typeof(XLEBridgeUtils.IShutdownWithEngine))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TerrainManipulator
        : IManipulator, IDisposable
        , XLEBridgeUtils.IShutdownWithEngine, XLEBridgeUtils.IManipulatorExtra
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
                if (    _attachedTerrainManiContext != null
                    &&  _attachedTerrainManiContext.ActiveLayer == 1001) {
                    return _decorationMaterialCombo;
                }
                return _baseTextureCombo;
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
        public TerrainManipulator(IContextRegistry contextRegistry, TerrainNamingBridge namingBridge)
        {
            _manipContext = new GUILayer.ActiveManipulatorContext();
            _manipContext.OnActiveManipulatorChange += 
                (object sender, EventArgs e) =>
                {
                    if (this.OnHoveringControlChanged != null)
                        this.OnHoveringControlChanged(null, EventArgs.Empty);
                };
            _nativeManip = null;
            _attachedSceneManager = new WeakReference(null);
            _attachedTerrainManiContext = null;

            _contextRegistry = new WeakReference(contextRegistry);
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
            _nativeManip = new GUILayer.NativeManipulatorLayer(_manipContext);

            _baseTextureCombo = new System.Windows.Forms.ComboBox();
            _baseTextureCombo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            _baseTextureCombo.DisplayMember = "Text";
            _baseTextureCombo.ValueMember = "Value";

            _decorationMaterialCombo = new System.Windows.Forms.ComboBox();
            _decorationMaterialCombo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            _decorationMaterialCombo.DisplayMember = "Text";
            _decorationMaterialCombo.ValueMember = "Value";

            _namingBridge = namingBridge;
            _namingBridge.OnBaseTextureMaterialsChanged += OnBaseTextureMaterialsChanged;
            _namingBridge.OnDecorationMaterialsChanged += OnDecorationMaterialsChanged;
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

            if (sceneMan == null || maniContext == null)
            {
                if (_attachedTerrainManiContext != null)
                    _attachedTerrainManiContext.OnActiveLayerChange -= OnActiveLayerChanged;
                _manipContext.ManipulatorSet = null;
                _attachedSceneManager.Target = null;
                _attachedTerrainManiContext = null;
                return;
            }

            if (sceneMan != _attachedSceneManager.Target || maniContext != _attachedTerrainManiContext)
            {
                if (_attachedTerrainManiContext != null)
                    _attachedTerrainManiContext.OnActiveLayerChange -= OnActiveLayerChanged;
                _manipContext.ManipulatorSet = sceneMan.CreateTerrainManipulators(maniContext);
                _attachedSceneManager.Target = sceneMan;
                _attachedTerrainManiContext = maniContext;
                if (_attachedTerrainManiContext != null)
                    _attachedTerrainManiContext.OnActiveLayerChange += OnActiveLayerChanged;
            }
        }

        private void OnBaseTextureMaterialsChanged(object sender, EventArgs e)
        {
            var selected = _manipContext.GetPaintCoverageMaterial();
            _baseTextureCombo.SelectedIndexChanged -= _baseTextureCombo_SelectedIndexChanged;
            _baseTextureCombo.Items.Clear();
            foreach (var m in _namingBridge.BaseTextureMaterials)
            {
                var idx = _baseTextureCombo.Items.Add(new { Text = m.Item1, Value = m.Item2 });
                if (m.Item2 == selected)
                    _baseTextureCombo.SelectedIndex = idx;
            }
            _baseTextureCombo.SelectedIndexChanged += _baseTextureCombo_SelectedIndexChanged;
        }

        private void OnDecorationMaterialsChanged(object sender, EventArgs e)
        {
            var selected = _manipContext.GetPaintCoverageMaterial();
            _decorationMaterialCombo.SelectedIndexChanged -= _decorationMaterialCombo_SelectedIndexChanged;
            _decorationMaterialCombo.Items.Clear();
            foreach (var m in _namingBridge.DecorationMaterials)
            {
                var idx = _decorationMaterialCombo.Items.Add(new { Text = m.Item1, Value = m.Item2 });
                if (m.Item2 == selected)
                    _decorationMaterialCombo.SelectedIndex = idx;
            }
            _decorationMaterialCombo.SelectedIndexChanged += _decorationMaterialCombo_SelectedIndexChanged;
        }

        private void OnActiveLayerChanged(object sender, EventArgs e)
        {
            // Update the "hovering control" as a result of the active coverage layer changing
            if (OnHoveringControlChanged != null)
                OnHoveringControlChanged(this, EventArgs.Empty);
        }

        void _baseTextureCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            dynamic o = _baseTextureCombo.SelectedItem;
            _manipContext.SetPaintCoverageMaterial((System.Int32)o.Value);
        }

        void _decorationMaterialCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            dynamic o = _decorationMaterialCombo.SelectedItem;
            _manipContext.SetPaintCoverageMaterial((System.Int32)o.Value);
        }

        private GUILayer.NativeManipulatorLayer _nativeManip;
        private GUILayer.ActiveManipulatorContext _manipContext;
        private WeakReference _contextRegistry;
        private TerrainNamingBridge _namingBridge;
        private System.Windows.Forms.ComboBox _baseTextureCombo;
        private System.Windows.Forms.ComboBox _decorationMaterialCombo;
        private WeakReference _attachedSceneManager;
        private GUILayer.TerrainManipulatorContext _attachedTerrainManiContext;
    }
}


