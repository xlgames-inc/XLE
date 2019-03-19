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

            _previewerContext = new ControlsLibraryExt.ModelView.PreviewerContext();
            _previewerControl = new ControlsLibraryExt.ModelView.PreviewerControl();
            // { Object = _settings, _activeMaterialContext = this._activeMaterialContext };
            _previewerControl.SetContext(_previewerContext);
            _controlHostService.RegisterControl(
                _previewerControl,
                new ControlInfo(
                    "Resource Preview".Localize(),
                    "Preview selected 3d resource".Localize(),
                    StandardControlGroup.Right),
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
            var resourceDesc = _resourceLister.LastSelected;
            if (!resourceDesc.HasValue) return;

            if ((resourceDesc.Value.Types & (uint)ResourceTypeFlags.Model) != 0)
            {
                // It's a model extension. Pass it through to the current settings object
                var name = _assetService.AsAssetName(resourceDesc.Value);
                var modelSettings = _previewerContext.ModelSettings;
                modelSettings.ModelName = name;
                modelSettings.MaterialName = name;
                modelSettings.Supplements = "";
                _previewerContext.ModelSettings = modelSettings;
                // _settings.ResetCamera = true;
                _previewerControl.Invalidate3DView();
            }

            if ((resourceDesc.Value.Types & (uint)ResourceTypeFlags.Animation) != 0)
            {
                var name = _assetService.AsAssetName(resourceDesc.Value);
                var modelSettings = _previewerContext.ModelSettings;
                modelSettings.AnimationFileName = name;
                _previewerContext.ModelSettings = modelSettings;
                _previewerControl.Invalidate3DView();
            }
        }

        [Import(AllowDefault = true)]
        private LevelEditorCore.ResourceLister _resourceLister = null;

        [Import(AllowDefault = false)]
        private ControlHostService _controlHostService = null;

        [Import(AllowDefault = true)]
        private ControlsLibraryExt.Material.ActiveMaterialContext _activeMaterialContext;
        private ControlsLibraryExt.ModelView.PreviewerControl _previewerControl;
        private ControlsLibraryExt.ModelView.PreviewerContext _previewerContext;

        private IEnumerable<string> _fileExtensions;

        [Import] private IXLEAssetService _assetService;
    }
}
