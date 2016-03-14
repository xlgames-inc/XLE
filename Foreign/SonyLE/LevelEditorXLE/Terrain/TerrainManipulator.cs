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
            _attachedTerrainManiContext = new WeakReference(null);

            _contextRegistry = new WeakReference(contextRegistry);
            contextRegistry.ActiveContextChanged += OnActiveContextChanged;
            _nativeManip = new GUILayer.NativeManipulatorLayer(_manipContext);

            _baseTextureCombo = new System.Windows.Forms.ComboBox();
            _baseTextureCombo.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            _baseTextureCombo.DisplayMember = "Text";
            _baseTextureCombo.ValueMember = "Value";

            _namingBridge = namingBridge;
            _namingBridge.OnBaseTextureMaterialsChanged += OnBaseTextureMaterialsChanged;
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

            if (sceneMan != null && maniContext != null)
            {
                if (sceneMan != _attachedSceneManager.Target || maniContext != _attachedTerrainManiContext.Target)
                {
                    _manipContext.ManipulatorSet = sceneMan.CreateTerrainManipulators(maniContext);
                    _attachedSceneManager.Target = sceneMan;
                    _attachedTerrainManiContext.Target = maniContext;
                }
            } 
            else 
            {
                _manipContext.ManipulatorSet = null;
                _attachedSceneManager.Target = null;
                _attachedTerrainManiContext.Target = null;
            }
        }

        private void OnBaseTextureMaterialsChanged(object sender, EventArgs e)
        {
            var selected = _baseTextureCombo.SelectedItem;
            _baseTextureCombo.SelectedIndexChanged -= _baseTextureCombo_SelectedIndexChanged;
            _baseTextureCombo.Items.Clear();
            foreach (var m in _namingBridge.BaseTextureMaterials)
                _baseTextureCombo.Items.Add(new { Text = m.Item1, Value = m.Item2 });
            _baseTextureCombo.SelectedItem = selected;
            _baseTextureCombo.SelectedIndexChanged += _baseTextureCombo_SelectedIndexChanged;
        }

        void _baseTextureCombo_SelectedIndexChanged(object sender, EventArgs e)
        {
            dynamic o = _baseTextureCombo.SelectedItem;
            _manipContext.SetTerrainBaseTextureMaterial((System.Int32)o.Value);
        }

        private GUILayer.NativeManipulatorLayer _nativeManip;
        private GUILayer.ActiveManipulatorContext _manipContext;
        private WeakReference _contextRegistry;
        private TerrainNamingBridge _namingBridge;
        private System.Windows.Forms.ComboBox _baseTextureCombo;
        private WeakReference _attachedSceneManager;
        private WeakReference _attachedTerrainManiContext;
    }
}


