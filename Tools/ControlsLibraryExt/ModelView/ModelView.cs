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
            set { _layerController.SetModelSettings(value); }
            get { return _layerController.GetModelSettings(); }
        }

        public GUILayer.VisOverlaySettings OverlaySettings
        {
            set { _layerController.SetOverlaySettings(value); }
            get { return _layerController.GetOverlaySettings(); }
        }
        public GUILayer.VisMouseOver MouseOver { get { return _layerController.MouseOver; } }
        public GUILayer.VisLayerController LayerController { get { return _layerController; } }

        private GUILayer.VisLayerController _layerController = new GUILayer.VisLayerController();
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
        }

        public void SetContext(PreviewerContext context)
        {
            var existingContext = ContextAs<PreviewerContext>();
            if (existingContext != null)
            {
                existingContext.LayerController.DetachFromView(_view.Underlying);
            }
            Context = context;
            context.LayerController.AttachToView(_view.Underlying);
            _ctrls.OverlaySettings = context.OverlaySettings;
            _ctrls.ModelSettings = context.ModelSettings;
        }

        public Material.ActiveMaterialContext ActiveMaterialContext { get; set; }

        public void Invalidate3DView() { _view.Invalidate(); }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _view.MouseClick -= OnViewerMouseClick;

                if (components != null) components.Dispose();
                if (_view != null) _view.Dispose();
            }
            base.Dispose(disposing);
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

    [Export(typeof(IInitializable))]
    [Export(typeof(ActiveModelView))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveModelView : IInitializable
    {
        void IInitializable.Initialize()
        {
            _sharedContext = new PreviewerContext();
            _controls = new PreviewerControl { Context = _sharedContext, ActiveMaterialContext = this._activeMaterialContext };
            _controlHostService.RegisterControl(
                _controls,
                new ControlInfo(
                    "Model view".Localize(), 
                    "Visualization of active preview model".Localize(), 
                    StandardControlGroup.Center),
                null);
        }

        public void LoadFromCommandLine(string model)
        {
            _sharedContext.ModelSettings.ModelName = model;
            _controls.Invalidate3DView();
        }

        public Control Control { get { return _controls; } }

        [Import(AllowDefault = false)] private IControlHostService _controlHostService;
        [Import(AllowDefault = true)] private Material.ActiveMaterialContext _activeMaterialContext;
        PreviewerControl _controls;
        PreviewerContext _sharedContext;
    }

    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PreviewerCommands : ICommandClient, IInitializable
    {
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            switch ((Command)commandTag)
            {
                case Command.SelectModel:
                    return _contextRegistry.GetActiveContext<PreviewerContext>() != null;
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.SelectModel:
                        var context = _contextRegistry.GetActiveContext<PreviewerContext>();
                        if (context != null)
                        {
                            var filters = GUILayer.Utils.GetModelExtensions();

                            var sb = new System.Text.StringBuilder("", 256);
                            bool first = true;
                            bool folderIsAnOption = false;
                            foreach (var f in filters)
                            {
                                if (f.Extension == "folder")
                                {
                                    folderIsAnOption = true;
                                    continue;
                                }

                                if (!first) sb.Append("|");
                                first = false;
                                sb.Append(f.Description);
                                sb.Append("|*.");
                                sb.Append(f.Extension);
                            }
                            if (!first) sb.Append("|");
                            first = false;
                            sb.Append("All Files|*.*");

                            ofd.Filter = sb.ToString();
                            if (ofd.ShowDialog() == DialogResult.OK)
                            {
                                var settings = context.ModelSettings;
                                settings.ModelName = ofd.FileName;
                                context.ModelSettings = settings;
                            }
                        }
                        break;
                }
            }
        }

        public void UpdateCommand(object commandTag, CommandState state) { }

        public virtual void Initialize()
        {
            _commandService.RegisterCommand(
                new CommandInfo(
                    Command.SelectModel,
                    StandardMenu.File,
                    "Previewer",
                    "Select Model".Localize(),
                    "Select model file to display in previewer".Localize(),
                    Sce.Atf.Input.Keys.None,
                    null,
                    CommandVisibility.Menu),
                this);
        }

        private enum Command
        {
            SelectModel
        }

        [Import(AllowDefault = false)]
        private ICommandService _commandService;

        [Import(AllowDefault = false)]
        private IContextRegistry _contextRegistry;

        private OpenFileDialog ofd = new OpenFileDialog();
    }
}
