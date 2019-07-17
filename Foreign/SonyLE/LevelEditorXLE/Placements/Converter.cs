// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel.Composition;
using System.ComponentModel;
using System.Xml.Serialization;
using System.Collections.Generic;
using System.Linq;
using System;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

using LevelEditorCore;

namespace LevelEditorXLE.Placements
{
    [Export(typeof(IResourceConverter))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceConverter : IResourceConverter
    {
        #region IResourceConverter Members
        IAdaptable IResourceConverter.Convert(IResource resource)
        {
            if (resource == null) return null;
            return XLEPlacementObject.Create(resource);
        }
        #endregion
    }

    [Export(typeof(LevelEditorCore.ResourceLister.ISubCommandClient))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceListerCommandClient : LevelEditorCore.ResourceLister.ISubCommandClient, IInitializable
    {
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return false;

            switch ((Command)commandTag)
            {
                case Command.SearchForPlacements:
                    {
                        var target = _resLister.ContextMenuTarget;
                        if (target == null) return false;
                        var ext = System.IO.Path.GetExtension(target);

                        // we can only do this for model files, or model bookmarks
                        return IsModelExtension(ext) || IsModelBookmarkExtension(ext);
                    }
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.SearchForPlacements:
                    {
                        var target = _resLister.ContextMenuTarget;
                        if (target == null) break;

                        IQueryPredicate predicate = null;

                        // note --  we could use the IResourceService to do the type resolution here by
                        //          calling IResourceService.Load... However, if the load operation is
                        //          expensive, we might not always want to do it.
                        var ext = System.IO.Path.GetExtension(target);
                        if (IsModelExtension(ext))
                        {
                            predicate = XLEPlacementObject.CreateSearchPredicate(new Uri(target, UriKind.Relative));
                        }
                        else if (IsModelBookmarkExtension(ext))
                        {
                            // todo -- we could load the bookmark via the resource service, as so -- 
                            // var res = IResourceService.Load(target);
                            // if (res == null) break;
                            var bookmark = XLEPlacementObject.LoadBookmark(new Uri(target, UriKind.Relative));
                            if (bookmark == null) break;
                            predicate = XLEPlacementObject.CreateSearchPredicate(bookmark, new Uri(target, UriKind.Relative));
                        }

                        if (predicate != null)
                        {
                            var queryContext = _contextRegistry.GetActiveContext<IQueryableContext>();
                            if (queryContext != null)
                            {
                                // note -- in our query context, the results will be displaced to the user automatically
                                // we could "show" the search results ui here, however.
                                queryContext.Query(predicate);

                                if (_searchService != null)
                                    _searchService.Show();
                            }
                        }
                    }
                    break;
            }
        }

        public void UpdateCommand(object commandTag, CommandState state) {}

        public System.Collections.Generic.IEnumerable<object> GetCommands(string focusUri)
        {
            return new System.Collections.Generic.List<object>{ Command.SearchForPlacements };
        }

        public void Initialize()
        {
            _commandService.RegisterCommand(
                Command.SearchForPlacements,
                null,
                "SearchCommands",
                "Search for placements".Localize(),
                "Find all objects that use this model".Localize(),
                this);
        }

        [ImportingConstructor]
        public ResourceListerCommandClient(IGameEngineProxy engineInfo)
        {
            if (engineInfo != null)
            {
                var modelType = engineInfo.Info.ResourceInfos.GetByType("Model");
                _modelFileExtensions = (modelType != null) ? modelType.FileExts : null;

                var bookmarkType = engineInfo.Info.ResourceInfos.GetByType("ModelBookmark");
                _modelBookmarkExtensions = (bookmarkType != null) ? bookmarkType.FileExts : null;
            }
            _modelFileExtensions = _modelFileExtensions ?? System.Linq.Enumerable.Empty<string>();
            _modelBookmarkExtensions = _modelBookmarkExtensions ?? System.Linq.Enumerable.Empty<string>();
        }

        private enum Command { SearchForPlacements }

        private bool IsModelExtension(string ext)
        {
            return _modelFileExtensions.Where(x => string.Equals(x, ext, StringComparison.OrdinalIgnoreCase)).FirstOrDefault() != null;
        }

        private bool IsModelBookmarkExtension(string ext)
        {
            return _modelBookmarkExtensions.Where(x => string.Equals(x, ext, StringComparison.OrdinalIgnoreCase)).FirstOrDefault() != null;
        }

        [Import(AllowDefault = false)]
        private ICommandService _commandService;

        [Import(AllowDefault = false)]
        private LevelEditorCore.ResourceLister _resLister;

        [Import(AllowDefault = false)]
        private IXLEAssetService _assetService;

        [Import(AllowDefault = false)]
        private IContextRegistry _contextRegistry;

        [Import(AllowDefault = true)]
        private ISearchService _searchService;

        private IEnumerable<string> _modelFileExtensions;
        private IEnumerable<string> _modelBookmarkExtensions;
    }
}
