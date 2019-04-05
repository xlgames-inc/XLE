using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text;

using Sce.Atf.Applications;

namespace MaterialTool
{
    [Export(typeof(ControlsLibraryExt.Material.ActiveMaterialContext))]
    [Export(typeof(NodeEditorCore.IPreviewMaterialContext))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveMaterialContext : ControlsLibraryExt.Material.ActiveMaterialContext, NodeEditorCore.IPreviewMaterialContext
    {
        public override IEnumerable<string> AssignableTechniqueConfigs 
        {
            get 
            {
                foreach (var d in _documentRegistry.Documents)
                {
                    // if the document is a DiagramDocument with a TechniqueConfig
                    // enabled, we can consider it a candidate technique config
                    var doc = d as DiagramDocument;
                    if (doc == null) continue;

                    var gc = doc.GraphMetaData;
                    if (gc.HasTechniqueConfig)
                    {
                        var cwd = new Uri(System.IO.Directory.GetCurrentDirectory().TrimEnd('\\') + "\\");
                        var relTo = new Uri(cwd, "xleres/techniques/");
                        var t = relTo.MakeRelativeUri(doc.Uri).OriginalString;
                        yield return t;
                    }
                }
            } 
        }

        string NodeEditorCore.IPreviewMaterialContext.ActivePreviewMaterialNames {
            get { return MaterialName; }
        }
         

        [Import] IDocumentRegistry _documentRegistry;
    }
}
