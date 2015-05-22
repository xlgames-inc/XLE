//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;

using LevelEditorCore;

using LevelEditor.DomNodeAdapters;

namespace LevelEditor
{
    [Export(typeof(IResourceConverter))]    
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceConverter : IResourceConverter
    {
        #region IResourceConverter Members

        IAdaptable IResourceConverter.Convert(IResource resource)
        {
            if (resource == null) return null;

            // <<XLE   --   disabling this conversion, because we want other converters
            //              to take presidence for this type
            // if (resource.Type == ResourceTypes.Model)
            // {
            //     Locator locator = Locator.Create();
            //     IReference<IResource> resRef = ResourceReference.Create(resource);                
            //     locator.Reference = resRef;
            //     locator.DomNode.InitializeExtensions();
            //     return locator;
            // }
            // XLE>>
 
            return null;
        }

        #endregion
    }
}
