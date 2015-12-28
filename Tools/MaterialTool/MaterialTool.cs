// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Controls.Adaptable.Graphs;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.VectorMath;
using Sce.Atf.Direct2D;
using Sce.Atf.Dom;

namespace MaterialTool
{
    [Export(typeof(IDocumentClient))]
    [Export(typeof(Editor))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class Editor : IDocumentClient, IControlHostClient, IInitializable
    {
        [ImportingConstructor]
        public Editor(
            IControlHostService controlHostService,
            ICommandService commandService,
            IContextRegistry contextRegistry,
            IDocumentRegistry documentRegistry,
            IDocumentService documentService,
            LayerLister layerLister)
        {
            m_controlHostService = controlHostService;
            m_commandService = commandService;
            m_contextRegistry = contextRegistry;
            m_documentRegistry = documentRegistry;
            m_documentService = documentService;
            m_layerLister = layerLister;

            // string initialDirectory = Path.Combine(Directory.GetCurrentDirectory(), "..\\..\\..\\..\\components\\wws_atf\\Samples\\CircuitEditor\\data");
            // EditorInfo.InitialDirectory = initialDirectory;
        }

        private IControlHostService m_controlHostService;
        private ICommandService m_commandService;
        private IContextRegistry m_contextRegistry;
        private IDocumentRegistry m_documentRegistry;
        private IDocumentService m_documentService;
        private LayerLister m_layerLister;

        [Import(AllowDefault = true)]
        private IStatusService m_statusService = null;
        [Import(AllowDefault = true)]
        private ISettingsService m_settingsService = null;
        [Import]
        private IFileDialogService m_fileDialogService = null;

        [ImportMany]
        private IEnumerable<Lazy<IContextMenuCommandProvider>> m_contextMenuCommandProviders = null;

        // scripting related members
        [Import(AllowDefault = true)]
        private ScriptingService m_scriptingService = null;

        #region IInitializable

        void IInitializable.Initialize()
        {
            if (m_scriptingService != null)
            {
                // load this assembly into script domain.
                m_scriptingService.LoadAssembly(GetType().Assembly);
                m_scriptingService.ImportAllTypes("CircuitEditorSample");
                m_scriptingService.ImportAllTypes("Sce.Atf.Controls.Adaptable.Graphs");

                m_scriptingService.SetVariable("editor", this);
                m_scriptingService.SetVariable("layerLister", m_layerLister);

                m_contextRegistry.ActiveContextChanged += delegate
                {
                    var editingContext = m_contextRegistry.GetActiveContext<CircuitEditingContext>();
                    ViewingContext viewContext = m_contextRegistry.GetActiveContext<ViewingContext>();
                    IHistoryContext hist = m_contextRegistry.GetActiveContext<IHistoryContext>();
                    m_scriptingService.SetVariable("editingContext", editingContext);
                    m_scriptingService.SetVariable("circuitContainer", editingContext != null ? editingContext.CircuitContainer : null);
                    m_scriptingService.SetVariable("view", viewContext);
                    m_scriptingService.SetVariable("hist", hist);
                };
            }

            if (m_settingsService != null)
            {
                // var settings = new[] 
                // {
                //   new BoundPropertyDescriptor(typeof (CircuitDefaultStyle),
                //         () => CircuitDefaultStyle.EdgeStyle,
                //         "Wire Style".Localize(), "Circuit Editor".Localize(),
                //         "Default Edge Style".Localize()),
                // };
                // m_settingsService.RegisterUserSettings("Circuit Editor", settings);
                // m_settingsService.RegisterSettings(this, settings);
            }
        }

        #endregion

        #region IDocumentClient Members

        public DocumentClientInfo Info { get { return EditorInfo; } }
        public static DocumentClientInfo EditorInfo =
            new DocumentClientInfo("Circuit".Localize(), ".circuit", null, null);

        public bool CanOpen(Uri uri) { return EditorInfo.IsCompatibleUri(uri); }

        public IDocument Open(Uri uri)
        {
            DomNode node = null;
            string filePath = uri.LocalPath;

            if (File.Exists(filePath))
            {
                
            }
            else
            {
                
            }

            CircuitDocument circuitCircuitDocument = null;
            if (node != null)
            {
                // now that the data is complete, initialize all other extensions to the Dom data
                node.InitializeExtensions();

                // AdaptableControl control = CreateCircuitControl(node);
                // control.AddHelp("https://github.com/SonyWWS/ATF/wiki/Adaptable-Controls".Localize());
                // 
                // var viewingContext = node.Cast<ViewingContext>();
                // viewingContext.Control = control;
                // 
                // circuitCircuitDocument = node.Cast<CircuitDocument>();
                // string fileName = Path.GetFileName(filePath);
                // ControlInfo controlInfo = new ControlInfo(fileName, filePath, StandardControlGroup.Center);
                // 
                // //Set IsDocument to true to prevent exception in command service if two files with the
                // //  same name, but in different directories, are opened.
                // controlInfo.IsDocument = true;
                // 
                // circuitCircuitDocument.ControlInfo = controlInfo;
                // circuitCircuitDocument.Uri = uri;

                // var editingContext = node.Cast<CircuitEditingContext>();
                // editingContext.GetLocalBound = GetLocalBound;
                // editingContext.GetWorldOffset = GetWorldOffset;
                // editingContext.GetTitleHeight = GetTitleHeight;
                // editingContext.GetLabelHeight = GetLabelHeight;
                // editingContext.GetSubContentOffset = GetSubContentOffset;
                // control.Context = editingContext;

                // m_circuitControlRegistry.RegisterControl(node, control, controlInfo, this);
                // 
                // // Set the zoom and translation to show the existing items (if any).
                // var enumerableContext = editingContext.Cast<IEnumerableContext>();
                // if (viewingContext.CanFrame(enumerableContext.Items))
                //     viewingContext.Frame(enumerableContext.Items);
            }

            return circuitCircuitDocument;
        }

        public void Show(IDocument document)
        {
            var viewingContext = document.Cast<ViewingContext>();
            m_controlHostService.Show(viewingContext.Control);
        }

        public void Save(IDocument document, Uri uri)
        {
            CircuitDocument circuitDocument = (CircuitDocument)document;
            string filePath = uri.LocalPath;
            FileMode fileMode = File.Exists(filePath) ? FileMode.Truncate : FileMode.OpenOrCreate;
            using (FileStream stream = new FileStream(filePath, fileMode))
            {
                // var writer = new CircuitWriter(m_schemaLoader.TypeCollection);
                // writer.Write(circuitDocument.DomNode, stream, uri);
            }
        }

        public void Close(IDocument document)
        {
            m_documentRegistry.Remove(document);
        }

        #endregion

        #region IControlHostClient Members

        public void Activate(Control control)
        {
            // AdaptableControl adaptableControl = (AdaptableControl)control;
            // var context = adaptableControl.ContextAs<CircuitEditingContext>();
            // m_contextRegistry.ActiveContext = context;
            // 
            // CircuitDocument circuitDocument = context.As<CircuitDocument>();
            // if (circuitDocument != null)
            //     m_documentRegistry.ActiveDocument = circuitDocument;
        }

        public void Deactivate(Control control) {}

        public bool Close(Control control)
        {
            var adaptableControl = (AdaptableControl)control;

            bool closed = true;
            CircuitDocument circuitDocument = adaptableControl.ContextAs<CircuitDocument>();
            if (circuitDocument != null)
            {
                closed = m_documentService.Close(circuitDocument);
                if (closed)
                    Close(circuitDocument);
            }
            else
            {
                // We don't care if the control was already unregistered. 'closed' should be true.
                m_circuitControlRegistry.UnregisterControl(control);
            }
            return closed;
        }

        #endregion

        /// <summary>
        /// Gets and sets a string to be used as the initial directory for the open/save dialog box
        /// regardless of whatever directory the user may have previously navigated to. The default
        /// value is null. Set to null to cancel this behavior.</summary>
        public string InitialDirectory
        {
            get { return m_fileDialogService.ForcedInitialDirectory; }
            set { m_fileDialogService.ForcedInitialDirectory = value; }
        }

        private void control_HoverStarted(object sender, HoverEventArgs<object, object> e)
        {
            m_hoverForm = GetHoverForm(e);
        }

        private HoverBase GetHoverForm(HoverEventArgs<object, object> e)
        {
            HoverBase result = CreateHoverForm(e);

            if (result != null)
            {
                Point p = Control.MousePosition;
                result.Location = new Point(p.X - (result.Width + 12), p.Y + 12);
                result.ShowWithoutFocus();
            }
            return result;
        }

        // create hover form for module or connection
        private HoverBase CreateHoverForm(HoverEventArgs<object, object> e)
        {
            // StringBuilder sb = new StringBuilder();
            // 
            // var hoverItem = e.Object;
            // var hoverPart = e.Part;
            // 
            // if (e.SubPart.Is<GroupPin>())
            // {
            //     sb.Append(e.SubPart.Cast<GroupPin>().Name);
            //     CircuitUtil.GetDomNodeName(e.SubPart.Cast<DomNode>());
            // }
            // else if (e.SubObject.Is<DomNode>())
            // {
            //     CircuitUtil.GetDomNodeName(e.SubObject.Cast<DomNode>());
            // }
            // else if (hoverPart.Is<GroupPin>())
            // {
            //     sb.Append(hoverPart.Cast<GroupPin>().Name);
            //     CircuitUtil.GetDomNodeName(hoverPart.Cast<DomNode>());
            // }
            // else if (hoverItem.Is<DomNode>())
            // {
            //     CircuitUtil.GetDomNodeName(hoverItem.Cast<DomNode>());
            // }
            // 
            // HoverBase result = null;
            // if (sb.Length > 0) // remove trailing '\n'
            // {
            //     //sb.Length = sb.Length - 1;
            //     result = new HoverLabel(sb.ToString());
            // }
            // 
            // return result;
            return null;
        }

        private void control_HoverStopped(object sender, EventArgs e)
        {
            if (m_hoverForm != null)
            {
                m_hoverForm.Controls.Clear();
                m_hoverForm.Close();
                m_hoverForm.Dispose();
            }
        }

        [Import]
        private CircuitControlRegistry m_circuitControlRegistry = null;
        private HoverBase m_hoverForm;

    }
}