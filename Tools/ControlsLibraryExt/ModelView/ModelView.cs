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

namespace ControlsLibraryExt.ModelView
{
    public partial class ModelView : UserControl
    {
        public ModelView()
        {
            InitializeComponent();

            _visResources = _view.Underlying.CreateVisResources();
            _ctrls.OnChange += (object sender, EventArgs args) => { _view.Invalidate(); };
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
                if (components != null) components.Dispose();
                if (_visMouseOver != null) _visMouseOver.Dispose();
                if (_visResources != null) _visResources.Dispose();
                if (_view != null) _view.Dispose();
            }
            base.Dispose(disposing);
        }

        private GUILayer.VisMouseOver _visMouseOver;
        private GUILayer.VisResources _visResources;
    }

    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveModelView : IInitializable
    {
        void IInitializable.Initialize()
        {
            _settings = GUILayer.ModelVisSettings.CreateDefault();
            _controls = new ModelView { Object = _settings };
            _controlHostService.RegisterControl(
                _controls,
                new ControlInfo(
                    "Model view".Localize(), 
                    "Visualization of active preview model".Localize(), 
                    StandardControlGroup.Center),
                null);
        }

        [Import(AllowDefault = false)] private IControlHostService _controlHostService;
        ModelView _controls;

        GUILayer.ModelVisSettings _settings;
    }
}
