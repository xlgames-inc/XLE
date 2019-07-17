//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Applications;
using PropertyGrid = Sce.Atf.Controls.PropertyEditing.PropertyGrid;

namespace LevelEditorCore
{
    /// <summary>
    /// Component to edit resource meta-data.
    /// </summary>
    [Export(typeof(IInitializable))]    
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ResourceMetadataEditor : IInitializable
    {
        public ResourceMetadataEditor()
        {
            m_propertyGrid = new PropertyGrid();
            m_controlInfo = new ControlInfo(
                "Resource Metadata".Localize(),
                "Edits selected resource metadata".Localize(),
                StandardControlGroup.Hidden);
        }

        #region IInitializable Members

        void IInitializable.Initialize()
        {
            if (m_resourceLister == null || m_resourceMetadataService == null)
                return;

            m_resourceLister.SelectionChanged += resourceLister_SelectionChanged;
            m_controlHostService.RegisterControl(m_propertyGrid, m_controlInfo, null);

        }

        #endregion

        private void resourceLister_SelectionChanged(object sender, EventArgs e)
        {
            var uris = new System.Collections.Generic.List<Uri>();
            foreach(var o in m_resourceLister.Selection)
            {
                var desc = resourceQueryService.GetDesc(o);
                if (!desc.HasValue) continue;
                uris.Add(new Uri(desc.Value.NaturalName, UriKind.Relative));
            }
            object[] mdatadata = m_resourceMetadataService.GetMetadata(uris).ToArray();
            m_propertyGrid.Bind(mdatadata);            
        }

        [Import(AllowDefault = true)]
        private ResourceLister m_resourceLister = null;

        [Import(AllowDefault = true)]
        private IResourceMetadataService m_resourceMetadataService = null;

        [Import(AllowDefault = true)]
        private IResourceQueryService resourceQueryService = null;

        [Import(AllowDefault = false)]
        private ControlHostService m_controlHostService = null;

        private readonly ControlInfo m_controlInfo;
        private readonly PropertyGrid m_propertyGrid;
    }
}
