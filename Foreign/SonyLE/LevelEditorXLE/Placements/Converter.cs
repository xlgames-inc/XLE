// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;

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

            if (XLEPlacementObject.CanReferenceStatic(resource))
            {
                var newPlacement = XLEPlacementObject.Create();
                newPlacement.Target = resource;
                return newPlacement;
            }
            return null;
        }

        #endregion
    }
}
