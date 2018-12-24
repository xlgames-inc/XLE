//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.IO;

using Sce.Atf;

using LevelEditorCore;

namespace RenderingInterop
{    
    [Export(typeof (IResourceResolver))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class AssetResolver : IResourceResolver
    {
        
        #region IResourceResolver Members

        public IResource Resolve(Uri uri)
        {
                // Very awkward here, because sometimes our "uri"s aren't
                // really URIs. They are just string identifiers that can
                // contain parameters extra information above just the 
                // basic path.
                // But the level editor stuff wants to work with URIs because
                // it will switch back and forth between relative and absolute
                // filenames.
            IResource resource = null;
            string fileName = uri.IsAbsoluteUri ? uri.LocalPath : uri.OriginalString;

            if (fileName.Substring(fileName.Length-6) == "<model")
            {
                resource = new ModelResource(
                    new Uri(fileName.Substring(0, fileName.Length-6)),
                    ResourceTypes.Model);
            }
            else
            {
                string ext = Path.GetExtension(fileName).ToLower();
                var res = m_gameEngine.Info.ResourceInfos.GetByType(ResourceTypes.Model);
                if (res.IsSupported(ext))
                {
                    resource = new ModelResource(uri, ResourceTypes.Model);
                } 
                else
                if (m_gameEngine.Info.ResourceInfos.GetByType("ModelBookmark").IsSupported(ext))
                {
                    resource = new ModelBookmarkResource(uri);
                }
            }

            return resource;
        }

        #endregion
        private class ModelResource : IResource
        {
            public ModelResource(Uri uri, string type)
            {
                m_uri = uri;
                m_type = type;
            }      
    
            #region IResource Members
            /// <summary>
            /// Gets a string identifying the type of the resource to the end-user</summary>
            public string Type
            {
                get { return m_type; }

            }

            /// <summary>
            /// Gets or sets the resource URI</summary>
            public Uri Uri
            {
                get { return m_uri; }
                set
                {
                    throw new InvalidOperationException();
                }
            }

            /// <summary>
            /// Event that is raised after the resource's URI changes</summary>
            public event EventHandler<UriChangedEventArgs> UriChanged
                = delegate { };
            
            #endregion
            private string m_type;
            private Uri m_uri;            
        }

        private class ModelBookmarkResource : IResource
        {
            public ModelBookmarkResource(Uri uri) { m_uri = uri; }
            public string Type { get { return "ModelBookmark"; } }

            public Uri Uri
            {
                get { return m_uri; }
                set { throw new InvalidOperationException(); }
            }

            public event EventHandler<UriChangedEventArgs> UriChanged = delegate { };

            private Uri m_uri;
        }

        [Import(AllowDefault=false)]
        private IGameEngineProxy m_gameEngine;
    }
}
