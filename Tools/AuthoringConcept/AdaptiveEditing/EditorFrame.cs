using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AuthoringConcept.AdaptiveEditing
{
    public class EditorFrame : ImmediateGUI.Frame
    {
        protected override void PerformLayout(ImmediateGUI.Arbiter gui, object context)
        {
            Declaration.PerformLayout(gui, Storage);
        }

        public IDataBlock Storage;
        public IDataBlockDeclaration Declaration;
    }
}
