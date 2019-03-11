using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Text;

using Sce.Atf.Applications;

namespace Previewer
{
    [Export(typeof(ControlsLibraryExt.Material.ActiveMaterialContext))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveMaterialContext : ControlsLibraryExt.Material.ActiveMaterialContext
    {
    }
}
