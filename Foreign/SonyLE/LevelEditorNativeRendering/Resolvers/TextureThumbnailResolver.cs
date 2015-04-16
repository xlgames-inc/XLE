//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using LevelEditorCore;
using Sce.Atf;
using Sce.Atf.Applications;

namespace RenderingInterop
{
    /// <summary>
    /// Generates thumbnail for dds and tga textures.
    /// </summary>
    [Export(typeof(IThumbnailResolver))]    
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class TextureThumbnailResolver : IThumbnailResolver
    {        
        #region IThumbnailResolver Members
        Image IThumbnailResolver.Resolve(Uri resourceUri)
        {
            return null;
        }
      
        private const float ThumbnailSize = 96;
        #endregion

        // [Import(AllowDefault = false)]
        // private IGameEngineProxy m_gameEngine;
    }
}
