// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;

using LevelEditorCore;

namespace LevelEditorXLE
{
    [Export(typeof(IResourceConverter))]    
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceConverter : IResourceConverter
    {
        #region IResourceConverter Members

        IAdaptable IResourceConverter.Convert(IResource resource)
        {
            if (resource == null) return null;
            if (resource.Type == ResourceTypes.Model)
            {
                var newPlacement = Placements.XLEPlacementObject.Create();
                if (newPlacement.CanReference(resource))
                {
                    newPlacement.Target = resource;
                    // return newPlacement;
                    System.Diagnostics.Debug.Assert(false, "incomplete implementation");
                    return null;
                }
            }
 
            return null;
        }

        #endregion
    }
}
