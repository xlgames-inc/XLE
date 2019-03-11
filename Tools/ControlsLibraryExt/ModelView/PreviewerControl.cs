// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Controls.Adaptable;

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace ControlsLibraryExt.ModelView
{
    [Export(typeof(PreviewerContext))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public class PreviewerContext
    {
        public GUILayer.ModelVisSettings ModelSettings
        {
            set
            {
                LayerController.SetModelSettings(value);
                if (OnModelSettingsChange != null)
                    OnModelSettingsChange.Invoke(this, null);
            }
            get { return LayerController.GetModelSettings(); }
        }

        public GUILayer.VisOverlaySettings OverlaySettings
        {
            set
            {
                LayerController.SetOverlaySettings(value);
                if (OnOverlaySettingsChange != null)
                    OnOverlaySettingsChange.Invoke(this, null);
            }
            get { return LayerController.GetOverlaySettings(); }
        }
        public GUILayer.VisMouseOver MouseOver { get { return LayerController.MouseOver; } }
        public GUILayer.VisLayerController LayerController { get; } = new GUILayer.VisLayerController();

        public event EventHandler OnModelSettingsChange;
        public event EventHandler OnOverlaySettingsChange;
    }

    [Export(typeof(PreviewerControl))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public partial class PreviewerControl : AdaptableControl
    {
        public PreviewerControl()
        {
            InitializeComponent();
            _view.MouseClick += OnViewerMouseClick;
            _ctrls.OverlaySettings_OnChange += (object sender, EventArgs args) => {
                var previewContext = ContextAs<PreviewerContext>();
                if (previewContext != null) {
                    previewContext.OverlaySettings = ((CtrlStrip)sender).OverlaySettings;
                }
                _view.Invalidate();
            };
            _ctrls.ModelSettings_OnChange += (object sender, EventArgs args) => {
                var previewContext = ContextAs<PreviewerContext>();
                if (previewContext != null)
                {
                    previewContext.ModelSettings = ((CtrlStrip)sender).ModelSettings;
                }
                _view.Invalidate();
            };
            _animationCtrls.OnInvalidateViews += (object sender, EventArgs args) => {
                _view.Invalidate();
            };
            _ctrls.OnResetCamera += (object sender, EventArgs args) => {
                var context = ContextAs<PreviewerContext>();
                if (context != null)
                {
                    context.LayerController.ResetCamera();
                }
            };
        }

        public void SetContext(PreviewerContext context)
        {
            var existingContext = ContextAs<PreviewerContext>();
            if (existingContext != null)
            {
                existingContext.LayerController.DetachFromView(_view.Underlying);
                existingContext.OnModelSettingsChange -= OnModelSettingsChange;
                existingContext.OnOverlaySettingsChange -= OnOverlaySettingsChange;
            }
            Context = context;
            if (context != null)
            {
                context.LayerController.AttachToView(_view.Underlying);
                _ctrls.OverlaySettings = context.OverlaySettings;
                _ctrls.ModelSettings = context.ModelSettings;
                _animationCtrls.AnimationState = context.LayerController.AnimationState;
                context.OnModelSettingsChange += OnModelSettingsChange;
                context.OnOverlaySettingsChange += OnOverlaySettingsChange;
            }
        }

        public Material.ActiveMaterialContext ActiveMaterialContext { get; set; }

        public void Invalidate3DView() { _view.Invalidate(); }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                SetContext(null);
                if (components != null) components.Dispose();
                if (_view != null)
                {
                    _view.MouseClick -= OnViewerMouseClick;
                    _view.Dispose();
                }
            }
            base.Dispose(disposing);
        }

        private void OnModelSettingsChange(object sender, EventArgs args)
        {
            var context = ContextAs<PreviewerContext>();
            if (context != null)
            {
                _ctrls.ModelSettings = context.ModelSettings;
            }
        }

        private void OnOverlaySettingsChange(object sender, EventArgs args)
        {
            var context = ContextAs<PreviewerContext>();
            if (context != null)
            {
                _ctrls.OverlaySettings = context.OverlaySettings;
            }
        }

        #region ContextMenu
        protected void ContextMenu_EditMaterial(object sender, EventArgs e)
        {
            if (ActiveMaterialContext == null) return;
            var i = sender as MenuItem;
            if (i != null)
            {
                var s = i.Tag as string;
                if (s != null)
                    ActiveMaterialContext.MaterialName = s;
            }
        }

        protected void ContextMenu_AssignTechnique(object sender, EventArgs e)
        {
            var i = sender as MenuItem;
            if (i != null)
            {
                var s = i.Tag as Tuple<string, string>;
                if (s != null) {
                    // Get this material, and change it's technique config to the one requested
                    string matName = s.Item1;
                    int lastSemi = matName.LastIndexOf(';');
                    if (lastSemi > 0) matName = matName.Substring(lastSemi + 1);
                    var mat = GUILayer.RawMaterial.Get(matName);
                    if (mat != null) {
                        mat.TechniqueConfig = s.Item2;
                        Invalidate();
                    }
                }
            }
        }

        protected void OnViewerMouseClick(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                var context = ContextAs<PreviewerContext>();
                if (context != null && context.MouseOver.HasMouseOver && ActiveMaterialContext != null)
                {
                    ContextMenu cm = new ContextMenu();
                    var matName = context.MouseOver.MaterialName;
                    if (!string.IsNullOrEmpty(matName) && matName[0] != '<')
                    {
                        cm.MenuItems.Add(
                            new MenuItem("Pick &Material (" + context.MouseOver.MaterialName + ")", new EventHandler(ContextMenu_EditMaterial)) { Tag = context.MouseOver.FullMaterialName });

                        foreach (var t in ActiveMaterialContext.AssignableTechniqueConfigs)
                        {
                            cm.MenuItems.Add(
                                new MenuItem("Assign Technique (" + t + ")",
                                new EventHandler(ContextMenu_AssignTechnique)) { Tag = new Tuple<string, string>(context.MouseOver.FullMaterialName, t) });
                        }
                    }

                    cm.Show(this, e.Location);
                }
            }
        }
        #endregion
    }
}
