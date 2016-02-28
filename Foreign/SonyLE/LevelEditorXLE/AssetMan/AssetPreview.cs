// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using System.IO;
using System.Linq;
using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Applications;
using LevelEditorCore;

using ModelView = ControlsLibraryExt.ModelView.ModelView;

namespace LevelEditorXLE.AssetMan
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourcePreview : IInitializable
    {
        #region IInitializable Members
        void IInitializable.Initialize()
        {
            if (_resourceLister == null) return;

            _resourceLister.SelectionChanged += resourceLister_SelectionChanged;

            _settings = GUILayer.ModelVisSettings.CreateDefault();
            _controls = new ModelView { Object = _settings, _activeMaterialContext = this._activeMaterialContext };
            _controlHostService.RegisterControl(
                _controls,
                new ControlInfo(
                    "Resource Preview".Localize(),
                    "Preview selected 3d resource".Localize(),
                    StandardControlGroup.Center),
                null);
        }
        #endregion

        [ImportingConstructor]
        public ResourcePreview(IGameEngineProxy engineInfo)
        {
            if (engineInfo != null)
            {
                var modelType = engineInfo.Info.ResourceInfos.GetByType("Model");
                if (modelType != null)
                    _fileExtensions = modelType.FileExts;
            }
            if (_fileExtensions == null)
                _fileExtensions = System.Linq.Enumerable.Empty<string>();
        }

        private void resourceLister_SelectionChanged(object sender, EventArgs e)
        {
            Uri resourceUri = _resourceLister.LastSelected;
            if (resourceUri == null) return;

            var ext = Path.GetExtension(resourceUri.AbsolutePath);
            if (_fileExtensions.Where(x => string.Equals(x, ext, StringComparison.OrdinalIgnoreCase)).FirstOrDefault() != null)
            {
                // It's a model extension. Pass it through to the current settings object
                var name = _assetService.StripExtension(_assetService.AsAssetName(resourceUri));
                _settings.ModelName = name;
                _settings.MaterialName = name;
                _settings.Supplements = "";
                _settings.ResetCamera = true;
                _controls.Invalidate3DView();
            }
        }

        [Import(AllowDefault = true)]
        private LevelEditorCore.ResourceLister _resourceLister = null;

        [Import(AllowDefault = false)]
        private ControlHostService _controlHostService = null;

        [Import(AllowDefault = true)]
        private ControlsLibraryExt.Material.ActiveMaterialContext _activeMaterialContext;
        private ModelView _controls;

        private GUILayer.ModelVisSettings _settings;

        private IEnumerable<string> _fileExtensions;

        [Import] private IXLEAssetService _assetService;
    }
}
