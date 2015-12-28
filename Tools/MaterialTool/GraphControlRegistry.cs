// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace MaterialTool
{
    /// <summary>
    /// Component  to  provide convenient service to register/unregister circuit controls created for circuit groups, 
    /// synchronize UI updates for circuit controls due to group renaming,  circuit element insertion/deletion,  
    /// and the closing of documents/controls. This only works if the IDocument can be adapted to a DomNode.</summary>
    [Export(typeof(GraphControlRegistry))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class GraphControlRegistry
    {
        [ImportingConstructor]
        public GraphControlRegistry(
            IControlHostService controlHostService,
            IContextRegistry contextRegistry,
            IDocumentService documentService)
        {
            _hostService = controlHostService;
            _contextRegistry = contextRegistry;
            documentService.DocumentOpened += documentService_DocumentOpened;
            documentService.DocumentClosed += documentService_DocumentClosed;
        }

        /// <summary>
        /// Registers the control for the circuit node</summary>
        /// <param name="circuitNode">Circuit DomNode</param>
        /// <param name="control">Control registered for DomNode</param>
        /// <param name="controlInfo">ControlInfo for control</param>
        /// <param name="client">Control IControlHostClient</param>
        public virtual void RegisterControl(GraphDocument doc, Control control, ControlInfo controlInfo, IControlHostClient client)
        {
            _controls.Add(doc, new Pair<Control, ControlInfo>(control, controlInfo));
            _hostService.RegisterControl(control, controlInfo, client);
        }

        /// <summary>
        /// Unregisters the Control from the context registry and closes and disposes it.</summary>
        /// <param name="control">Control</param>
        /// <returns>True if the Control was previously passed in to RegisterControl. False if
        /// the Control was unrecognized in which case no change was made.</returns>
        public virtual bool UnregisterControl(Control control)
        {
            bool result = false;
            var matched = _controls.FirstOrDefault(n => n.Value.First == control);
            if (matched.Key != null)
            {
                UnregisterControl(matched.Key, matched.Value.First);
                result = true;
            }
            return result;
        }

        /// <summary>
        /// Retrieve all the DomNode/circuit-control pairs currently registered </summary>
        public IEnumerable< KeyValuePair<GraphDocument, Pair<Control, ControlInfo>>> CircuitNodeControls
        {
            get { return _controls; }

        }

        /// <summary>
        /// Get the ControlInfo of the circuit control associated with the DomNode</summary>
        /// <param name="domNode">DomNode to obtain ControlInfo for</param>
        /// <returns>ControlInfo associated with DomNode</returns>
        public ControlInfo GetCircuitControlInfo(GraphDocument domNode)
        {
            return (from ctrol in _controls where ctrol.Key == domNode select ctrol.Value.Second).FirstOrDefault();
        }
        
        /// <summary>
        ///  Get the associated DomNode for the circuit control</summary>
        ///  <param name="control">Circuit control</param>
        ///  <returns>DomNode associated with circuit control</returns>
        public GraphDocument GetDomNode(Control control)
        {
            return (from ctrol in _controls where ctrol.Value.Second.Control == control select ctrol.Key).FirstOrDefault();
        }

        /// <summary>
        /// Unregisters the Control from the IContextRegistry and IControlHostService and disposes
        /// it and sets the circuitNode's ViewingContext's Control property to null.</summary>
        private void UnregisterControl(GraphDocument circuitNode, Control control)
        {
            //it's OK if the CircuitEditingContext was already removed or wasn't added to IContextRegistry.
            _contextRegistry.RemoveContext(circuitNode.As<GraphEditingContext>());
            _hostService.UnregisterControl(control);
            control.Visible = false;
            control.Dispose();
            _controls.Remove(circuitNode);
            // circuitNode.Cast<ViewingContext>().Control = null;
        }

        private void documentService_DocumentOpened(object sender, DocumentEventArgs e)
        {
        }

        private void documentService_DocumentClosed(object sender, DocumentEventArgs e)
        {
            if (e.Document.Is<GraphDocument>())
            {
                foreach (var circuitControl in _controls.ToArray())
                {
                    if (circuitControl.Key == e.Document)
                    {
                        UnregisterControl(circuitControl.Key, circuitControl.Value.First);
                    }
                }
            }
        }

        private void CloseEditingContext(GraphEditingContext editingContext)
        {
            _contextRegistry.RemoveContext(editingContext);

            // if (editingContext.Is<ViewingContext>())
            // {
            //     var viewingContext = editingContext.Cast<ViewingContext>();
            // 
            //     if (viewingContext.Control != null)
            //     {
            //         UnregisterControl(viewingContext.DomNode, viewingContext.Control);
            //         viewingContext.Control = null;
            //     }
            // }
        }

        private readonly IControlHostService _hostService;
        private readonly IContextRegistry _contextRegistry;
        private readonly Dictionary<GraphDocument, Pair<Control, ControlInfo>> _controls = new Dictionary<GraphDocument, Pair<Control, ControlInfo>>();
     }
}
