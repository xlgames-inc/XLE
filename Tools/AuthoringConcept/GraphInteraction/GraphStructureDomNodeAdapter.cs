using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Sce.Atf;
using Sce.Atf.Dom;
using Sce.Atf.Adaptation;


namespace AuthoringConcept
{
    class GraphStructureDomNodeAdapter : DomNodeAdapter
    {
        public string Structure
        {
            get
            {
                return DomNode.GetAttribute(_structureAttribute) as string;
            }

            set
            {
                DomNode.SetAttribute(_structureAttribute, value);
            }
        }

        public HyperGraph.IGraphModel ViewModel
        {
            get { return _viewModel; }
            set
            {
                if (_viewModel != null)
                {
                    _viewModel.NodeAdded -= _viewModel_NodeAdded;
                    _viewModel.NodeRemoved -= _viewModel_NodeRemoved;
                    _viewModel.ConnectionAdded -= _viewModel_ConnectionAdded;
                    _viewModel.ConnectionRemoved -= _viewModel_ConnectionRemoved;
                }

                _viewModel = value;

                if (_viewModel != null)
                {
                    _viewModel.NodeAdded += _viewModel_NodeAdded;
                    _viewModel.NodeRemoved += _viewModel_NodeRemoved;
                    _viewModel.ConnectionAdded += _viewModel_ConnectionAdded;
                    _viewModel.ConnectionRemoved += _viewModel_ConnectionRemoved;

                    UpdateStructure();
                }
            }
        }

        private void UpdateStructure()
        {
            var model = ModelConversion.ToNodeGraphFile(ViewModel, false);
            var sg = model.SubGraphs.FirstOrDefault().Value;
            if (sg != null)
            {
                var str = sg.Graph.Print(sg.Signature, model.SubGraphs.FirstOrDefault().Key);
                if (String.Compare(str, Structure) != 0)
                    Structure = str;
            }
        }

        private void _viewModel_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e)
        {
            UpdateStructure();
        }

        private void _viewModel_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e)
        {
            UpdateStructure();
        }

        private void _viewModel_NodeRemoved(object sender, HyperGraph.NodeEventArgs e)
        {
            UpdateStructure();
        }

        private void _viewModel_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e)
        {
            UpdateStructure();
        }

        protected override void OnNodeSet()
        {
            base.OnNodeSet();
            _structureAttribute = DomNode.Type.GetAttributeInfo("Structure");
        }

        private AttributeInfo _structureAttribute;
        private HyperGraph.IGraphModel _viewModel;
    }
}
