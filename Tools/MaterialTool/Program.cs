// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;
using System.Drawing;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls.Adaptable;
using Sce.Atf.Controls.Adaptable.Graphs;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Dom;

namespace MaterialTool
{
    public class Program
    {
        [STAThread]
        static void Main()
        {
            // This startup sequence is based on the "Circuit editor" sample in the Sony ATF repo
            // We want to take advantage of the ATF implementations whereever they exist, but we'll
            // remove the parts that are specific to that circuit editor sample app.

            // It's important to call these before starting the app; otherwise theming and bitmaps
            //  may not render correctly.
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.DoEvents(); // see http://www.codeproject.com/buglist/EnableVisualStylesBug.asp?df=100&forumid=25268&exp=0&select=984714

            // Set up localization support early on, so that user-readable strings will be localized
            //  during the initialization phase below. Use XML files that are embedded resources.
            Thread.CurrentThread.CurrentUICulture = System.Globalization.CultureInfo.CurrentCulture;
            Localizer.SetStringLocalizer(new EmbeddedResourceStringLocalizer());
            
            // early engine initialization
            var engineDevice = new GUILayer.EngineDevice();
            GC.KeepAlive(engineDevice);

            // Enable metadata driven property editing for the DOM
            DomNodeType.BaseOfAllTypes.AddAdapterCreator(new AdapterCreator<CustomTypeDescriptorNodeAdapter>());

            // Create a type catalog with the types of components we want in the application
            var catalog = new TypeCatalog(

                typeof(SettingsService),                // persistent settings and user preferences dialog
                // typeof(StatusService),                  // status bar at bottom of main Form
                typeof(CommandService),                 // handles commands in menus and toolbars
                typeof(ControlHostService),             // docking control host
                typeof(WindowLayoutService),            // multiple window layout support
                typeof(WindowLayoutServiceCommands),    // window layout commands
                // typeof(AtfUsageLogger),                 // logs computer info to an ATF server
                // typeof(CrashLogger),                    // logs unhandled exceptions to an ATF server
                // typeof(UnhandledExceptionService),      // catches unhandled exceptions, displays info, and gives user a chance to save
                typeof(FileDialogService),              // standard Windows file dialogs

                typeof(DocumentRegistry),               // central document registry with change notification
                typeof(AutoDocumentService),            // opens documents from last session, or creates a new document, on startup
                typeof(RecentDocumentCommands),         // standard recent document commands in File menu
                typeof(StandardFileCommands),           // standard File menu commands for New, Open, Save, SaveAs, Close
                typeof(MainWindowTitleService),         // tracks document changes and updates main form title
                typeof(TabbedControlSelector),          // enable ctrl-tab selection of documents and controls within the app
                typeof(HelpAboutCommand),               // Help -> About command

                typeof(ContextRegistry),                // central context registry with change notification
                typeof(StandardFileExitCommand),        // standard File exit menu command
                // typeof(StandardEditCommands),           // standard Edit menu commands for copy/paste
                // typeof(StandardEditHistoryCommands),    // standard Edit menu commands for undo/redo
                // typeof(StandardSelectionCommands),      // standard Edit menu selection commands
                typeof(StandardLayoutCommands),         // standard Format menu layout commands
                typeof(StandardViewCommands),           // standard View menu commands

                // typeof(PaletteService),                 // global palette, for drag/drop instancing

                typeof(PropertyEditor),                 // property grid for editing selected objects
                typeof(GridPropertyEditor),             // grid control for editing selected objects
                typeof(PropertyEditingCommands),        // commands for PropertyEditor and GridPropertyEditor, like Reset,
                                                        // Reset All, Copy Value, Paste Value, Copy All, Paste All

                typeof(HistoryLister),                  // visual list of undo/redo stack
                typeof(PrototypeLister),                // editable palette of instantiable item groups
                typeof(LayerLister),                    // editable tree view of layers

                typeof(Outputs),                        // passes messages to all log writers
                // typeof(ErrorDialogService),             // displays errors to the user in a message box
                typeof(OutputService),                  // rich text box for displaying error and warning messages. Implements IOutputWriter.
                typeof(DomRecorder),                    // records and displays changes to the DOM for diagnostic purposes

                typeof(Editor),                         // editor which manages circuit documents and controls
                // typeof(SchemaLoader),                   // loads circuit schema and extends types
                // typeof(GroupingCommands),               // circuit group/ungroup commands
                typeof(DiagramControlRegistry),         // circuit controls management
                // typeof(LayeringCommands),               // "Add Layer" command
                // typeof(GraphViewCommands),              // zooming with presets
                typeof(PerformanceMonitor),             // displays the frame rate and memory usage
                typeof(DefaultTabCommands),             // provides the default commands related to document tab Controls
                // typeof(ModulePlugin),                   // component that defines circuit module types
                // typeof(TemplateLister),                 // template library for subgraph referencing or instancing 
                // typeof(TemplatingCommands),             // commands for promoting/demoting graph elements to/from template library 
                //typeof(TemplatingSupervisor),         // templated instances copy-on-edit support(optionally)

                typeof(AnnotatingCommands),             // annotating commands
                // typeof(CircuitTestCommands),            // circuit tester commands

                typeof(PythonService),                  // scripting service for automated tests
                typeof(ScriptConsole),                  // provides a dockable command console for entering Python commands
                typeof(AtfScriptVariables),             // exposes common ATF services as script variables
                typeof(AutomationService),              // provides facilities to run an automated script using the .NET remoting service

                typeof(SkinService),

                typeof(ShaderPatcherLayer.Manager),
                typeof(ShaderFragmentArchive.Archive),
               
                typeof(DiagramControl),
                typeof(ShaderFragmentArchiveControl),

                typeof(NodeEditorCore.ShaderFragmentArchiveModel),
                typeof(NodeEditorCore.ModelConversion),
                typeof(NodeEditorCore.ShaderFragmentNodeCreator),
                typeof(NodeEditorCore.DiagramDocument),

                typeof(ControlsLibraryExt.Material.ActiveMaterialContext),
                typeof(ControlsLibraryExt.Material.MaterialInspector),
                typeof(ControlsLibraryExt.Material.MaterialSchemaLoader),

                typeof(ControlsLibraryExt.ModelView.ActiveModelView)
            );

            // enable use of the system clipboard
            StandardEditCommands.UseSystemClipboard = true;

            // Set up the MEF container with these components
            var container = new CompositionContainer(catalog);

            // This is bit wierd, but we're going to add the container to itself.
            // This will create a tight circular dependency, of course
            // It's also not ideal by the core DI pattern. 
            // But it's useful for us, because we want to use the same container to construct
            // objects (and also to retrieve global instances).
            container.ComposeExportedValue<ExportProvider>(container);
            container.ComposeExportedValue<CompositionContainer>(container);

            // Configure the main Form
            var batch = new CompositionBatch();
            var mainForm = new MainForm(new ToolStripContainer())
                {
                    Text = Application.ProductName //,
                    // Icon = GdiUtil.CreateIcon(ResourceUtil.GetImage(Sce.Atf.Resources.AtfIconImage))
                };
            // Sce.Atf.Direct2D.D2dFactory.EnableResourceSharing(mainForm.Handle);

            // Add the main Form instance, etc., to the container
            batch.AddPart(mainForm);
            // batch.AddPart(new WebHelpCommands("https://github.com/SonyWWS/ATF/wiki/ATF-Circuit-Editor-Sample".Localize()));
            container.Compose(batch);

            // We need to attach compilers for models, etc
            engineDevice.AttachDefaultCompilers();

            container.InitializeAll();
            Application.Run(mainForm);
            container.Dispose();
            engineDevice.Dispose();
        }
    }
}
