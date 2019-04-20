using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace AuthoringConcept
{
    [Export(typeof(DiagramEditingContext))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public class DiagramEditingContext : HyperGraphAdapter.DiagramEditingContext
    {
        public IDocument Document { get; set; }

        public DiagramEditingContext()
        {
            ViewModel = new HyperGraph.GraphModel();
            ViewModel.CompatibilityStrategy = new HyperGraph.Compatibility.AlwaysCompatible();

            // GetDescription = o => (NodeFactory != null) ? NodeFactory.GetDescription(o) : string.Empty;
        }

        #region ViewModel Changes
        protected override void model_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e)
        {
            base.model_NodeAdded(sender, e);
            Document.Dirty = true;
        }

        protected override void model_NodeRemoved(object sender, HyperGraph.NodeEventArgs e)
        {
            base.model_NodeRemoved(sender, e);
            Document.Dirty = true;
        }

        protected override void model_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e)
        {
            base.model_ConnectionAdded(sender, e);
            Document.Dirty = true;
        }

        protected override void model_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e)
        {
            base.model_ConnectionRemoved(sender, e);
            Document.Dirty = true;
        }

        protected override void model_MiscChange(object sender, HyperGraph.MiscChangeEventArgs e)
        {
            base.model_MiscChange(sender, e);
            Document.Dirty = true;
        }
        #endregion

        private static Color s_zeroColor = new Color();
    }
}
