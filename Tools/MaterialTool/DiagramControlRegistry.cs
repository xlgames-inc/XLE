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
    [Export(typeof(DiagramControlRegistry))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class DiagramControlRegistry
    {
        [ImportingConstructor]
        public DiagramControlRegistry(
            IControlHostService controlHostService,
            IContextRegistry contextRegistry,
            IDocumentService documentService)
        {
            _hostService = controlHostService;
            _contextRegistry = contextRegistry;
            documentService.DocumentOpened += documentService_DocumentOpened;
            documentService.DocumentClosed += documentService_DocumentClosed;
        }

        public virtual void RegisterControl(IDocument doc, Control control, ControlInfo controlInfo, IControlHostClient client)
        {
            _controls.Add(doc, new Pair<Control, ControlInfo>(control, controlInfo));
            _hostService.RegisterControl(control, controlInfo, client);
        }

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

        public IEnumerable< KeyValuePair<IDocument, Pair<Control, ControlInfo>>> DiagramControls
        {
            get { return _controls; }
        }

        public ControlInfo GetControlInfo(IDocument domNode)
        {
            return (from ctrol in _controls where ctrol.Key == domNode select ctrol.Value.Second).FirstOrDefault();
        }
        
        public IDocument GetDomNode(Control control)
        {
            return (from ctrol in _controls where ctrol.Value.Second.Control == control select ctrol.Key).FirstOrDefault();
        }

        private void UnregisterControl(IDocument circuitNode, Control control)
        {
            //it's OK if the CircuitEditingContext was already removed or wasn't added to IContextRegistry.
            _contextRegistry.RemoveContext(circuitNode.As<DiagramEditingContext>());
            _hostService.UnregisterControl(control);
            control.Visible = false;
            control.Dispose();
            _controls.Remove(circuitNode);
        }

        private void documentService_DocumentOpened(object sender, DocumentEventArgs e)
        {
        }

        private void documentService_DocumentClosed(object sender, DocumentEventArgs e)
        {
            if (e.Document.Is<IDocument>())
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

        private readonly IControlHostService _hostService;
        private readonly IContextRegistry _contextRegistry;
        private readonly Dictionary<IDocument, Pair<Control, ControlInfo>> _controls = new Dictionary<IDocument, Pair<Control, ControlInfo>>();
     }
}
