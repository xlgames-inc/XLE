//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.ComponentModel.Composition;
using Sce.Atf;

using LevelEditorCore;

namespace LevelEditorXLE
{
    [Export(typeof(IResourceConverter))]    
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceConverter : IResourceConverter
    {
        #region IResourceConverter Members

        IGameObject IResourceConverter.Convert(IResource resource)
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
