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

#pragma warning disable 0649        // Field '...' is never assigned to, and will always have its default value null

namespace ControlsLibraryExt.ModelView
{
    public partial class ModelView : UserControl
    {
        public ModelView()
        {
            InitializeComponent();

            _visResources = _view.Underlying.CreateVisResources();
            _ctrls.OnChange += (object sender, EventArgs args) => { _view.Invalidate(); };

            _view.MouseClick += OnViewerMouseClick;
        }

        public GUILayer.ModelVisSettings Object
        {
            set 
            {
                _ctrls.Object = value; 
                _visMouseOver = _view.Underlying.CreateVisMouseOver(value, _visResources);
                _view.Underlying.SetupDefaultVis(value, _visMouseOver, _visResources);
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _view.MouseClick -= OnViewerMouseClick;

                if (components != null) components.Dispose();
                if (_visMouseOver != null) _visMouseOver.Dispose();
                if (_visResources != null) _visResources.Dispose();
                if (_view != null) _view.Dispose();
            }
            base.Dispose(disposing);
        }

        #region ContextMenu
        protected void ContextMenu_EditMaterial(object sender, EventArgs e)
        {
            if (_activeMaterialContext == null) return;
            var i = sender as MenuItem;
            if (i != null)
            {
                var s = i.Tag as string;
                if (s != null)
                    _activeMaterialContext.MaterialName = s;
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
                if (_visMouseOver.HasMouseOver && _activeMaterialContext != null)
                {
                    ContextMenu cm = new ContextMenu();
                    cm.MenuItems.Add(
                        new MenuItem("Pick &Material (" + _visMouseOver.MaterialName + ")", new EventHandler(ContextMenu_EditMaterial))
                        { Tag = _visMouseOver.FullMaterialName });

                    foreach (var t in _activeMaterialContext.AssignableTechniqueConfigs)
                    {
                        cm.MenuItems.Add(
                            new MenuItem("Assign Technique (" + t + ")", 
                            new EventHandler(ContextMenu_AssignTechnique)) { Tag = new Tuple<string, string>(_visMouseOver.FullMaterialName, t) });
                    }

                    cm.Show(this, e.Location);
                }
            }
        }
        #endregion

        private GUILayer.VisMouseOver _visMouseOver;
        private GUILayer.VisResources _visResources;

        internal Material.ActiveMaterialContext _activeMaterialContext;
    }

    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveModelView : IInitializable
    {
        void IInitializable.Initialize()
        {
            _settings = GUILayer.ModelVisSettings.CreateDefault();
            _controls = new ModelView { Object = _settings, _activeMaterialContext = this._activeMaterialContext };
            _controlHostService.RegisterControl(
                _controls,
                new ControlInfo(
                    "Model view".Localize(), 
                    "Visualization of active preview model".Localize(), 
                    StandardControlGroup.Center),
                null);
        }

        [Import(AllowDefault = false)] private IControlHostService _controlHostService;
        [Import] private Material.ActiveMaterialContext _activeMaterialContext;
        ModelView _controls;

        GUILayer.ModelVisSettings _settings;
    }
}
