using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text;

using Sce.Atf.Applications;

namespace MaterialTool
{
    [Export(typeof(ControlsLibraryExt.Material.ActiveMaterialContext))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveMaterialContext : ControlsLibraryExt.Material.ActiveMaterialContext
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

                    var gc = doc.UnderlyingDocument.GraphContext;
                    if (gc.HasTechniqueConfig)
                    {
                        var relTo = new Uri(System.IO.Directory.GetCurrentDirectory() + "/game/xleres/techniques/");
                        var t = relTo.MakeRelativeUri(doc.Uri).OriginalString;
                        var e = t.LastIndexOf('.'); // we must remove the extension... Using simple string parsing
                        if (e > 0) t = t.Substring(0, e);
                        yield return t;
                    }
                }
            } 
        }

        [Import] IDocumentRegistry _documentRegistry;
    }
}
