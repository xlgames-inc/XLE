// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace MaterialTool
{
    [Export(typeof(DiagramEditingContext))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public class DiagramEditingContext :
        HyperGraphAdapter.DiagramEditingContext
        , NodeEditorCore.IEditingContext
    {
        private class ViewModelAdapter : DiagramDocument.IViewModel
        {
            public uint RevisionIndex { get { return ViewModel.GlobalRevisionIndex; } }
            public GUILayer.NodeGraphFile Rebuild()
            {
                return ModelConversion.ToShaderPatcherLayer(ViewModel);
            }
            public HyperGraph.IGraphModel ViewModel { get; set; }
            public NodeEditorCore.ModelConversion ModelConversion;
        }

        public NodeEditorCore.IDiagramDocument Document
        {
            get { return _document; }
            set {
                if (_document is DiagramDocument diagDoc)
                    diagDoc.ViewModel = null;

                ViewModel = null;
                _document = value;

                if (_document != null)
                {
                    ViewModel = new HyperGraph.GraphModel();
                    ViewModel.CompatibilityStrategy = NodeFactory.CreateCompatibilityStrategy();
                    ModelConversion.AddToHyperGraph(_document.NodeGraphFile, ViewModel);

                    if (_document is DiagramDocument diagDoc2)
                    {
                        diagDoc2.ViewModel = new ViewModelAdapter { ViewModel = ViewModel, ModelConversion = ModelConversion };
                    }
                }
            }
        }

        public DiagramEditingContext()
        {
            GetDescription = o => (NodeFactory != null) ? NodeFactory.GetDescription(o) : string.Empty;
        }

        ~DiagramEditingContext()
        {
            Document = null;  // (remove callbacks)
        }

        #region ViewModel Changes
        protected override void model_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e)
        {
            base.model_NodeAdded(sender, e);
            (Document as IDocument).Dirty = true;
        }

        protected override void model_NodeRemoved(object sender, HyperGraph.NodeEventArgs e)
        {
            base.model_NodeRemoved(sender, e);
            (Document as IDocument).Dirty = true;
        }

        protected override void model_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e)
        {
            base.model_ConnectionAdded(sender, e);
            (Document as IDocument).Dirty = true;

            // Always update the node entirely if the instantiation connector affected
            if (NodeFactory.IsInstantiationConnector(e.Connection.To))
            {
                NodeFactory.UpdateProcedureNode(Document.NodeGraphFile, e.Connection.To.Node);
            }
            else if (NodeFactory.IsInstantiationConnector(e.Connection.From))
            {
                NodeFactory.UpdateProcedureNode(Document.NodeGraphFile, e.Connection.From.Node);
            }
        }

        protected override void model_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e)
        {
            base.model_ConnectionRemoved(sender, e);
            (Document as IDocument).Dirty = true;

            // Always update the node entirely if the instantiation connector affected
            if (NodeFactory.IsInstantiationConnector(e.To))
            {
                NodeFactory.UpdateProcedureNode(Document.NodeGraphFile, e.To.Node);
            }
            else if (NodeFactory.IsInstantiationConnector(e.From))
            {
                NodeFactory.UpdateProcedureNode(Document.NodeGraphFile, e.From.Node);
            }
        }

        protected override void model_MiscChange(object sender, HyperGraph.MiscChangeEventArgs e)
        {
            base.model_MiscChange(sender, e);
            (Document as IDocument).Dirty = true;
        }
        #endregion

        [Import] private NodeEditorCore.ShaderFragmentNodeCreator NodeFactory;
        [Import] private NodeEditorCore.ModelConversion ModelConversion;
        private NodeEditorCore.IDiagramDocument _document;

        private static Color s_zeroColor = new Color();
    }
}
