//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Adaptation;

using LevelEditorCore;
using LevelEditor.DomNodeAdapters;
using Sce.Atf.Dom;

namespace LevelEditor
{
    /// <summary>
    /// Editor providing a hierarchical tree control, listing the contents of a loaded document.</summary>
    [Export(typeof (IInitializable))]
    [Export(typeof(GameProjectLister))]
    [Export(typeof (IContextMenuCommandProvider))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class GameProjectLister : FilteredTreeControlEditor, IControlHostClient, IInitializable,
                                     IContextMenuCommandProvider, ICommandClient
    {
        [ImportingConstructor]
        public GameProjectLister(
            ICommandService commandService,
            IControlHostService controlHostService,
            IContextRegistry contextRegistry)
            : base(commandService)
        {
            m_controlHostService = controlHostService;
            m_contextRegistry = contextRegistry;                        
            m_contextRegistry.ActiveContextChanged += ContextRegistry_ActiveContextChanged;
        }

       

        /// <summary>
        /// Refreshes project tree.
        /// </summary>
        public void Refresh()
        {
            if(TreeView  != null)
            {
               TreeControlAdapter.Refresh(TreeView.Root);
            }
        }
        
        #region IInitializable Members

        public void Initialize()
        {
            
            string addNewSubGame = "Add new SubGame".Localize();
            CommandService.RegisterCommand(
               Command.CreateNewSubGame,
               StandardMenu.File,
               StandardCommandGroup.FileNew,
               addNewSubGame,
               addNewSubGame,
               Keys.None,
               null,
               CommandVisibility.ContextMenu,
               this);

            string addExistingEubGame = "Add existing SubGame".Localize();
            CommandService.RegisterCommand(
                Command.AddSubGame,
               StandardMenu.File,
               StandardCommandGroup.FileNew,
                addExistingEubGame,
                addExistingEubGame,
                Keys.None,
                null,
                CommandVisibility.ContextMenu,
                this);

            //<<XLE
            string setupWorldPlacements = "Setup world placements".Localize();
            CommandService.RegisterCommand(
               Command.SetupWorldPlacements,
               StandardMenu.File,
               StandardCommandGroup.FileNew,
               setupWorldPlacements,
               setupWorldPlacements,
               Keys.None,
               null,
               CommandVisibility.ContextMenu,
               this);
            //XLE>>

            CommandService.RegisterCommand(
               Command.Exclude,
              StandardMenu.File,
              StandardCommandGroup.FileNew,
               "Exclude SubGame".Localize(),
               "Exlude from master level".Localize(),
               Keys.None,
               null,
               CommandVisibility.ContextMenu,
               this);


            string resolveSubGame = "Resolve SubGame".Localize();
            CommandService.RegisterCommand(
               Command.Resolve,
              StandardMenu.File,
              StandardCommandGroup.FileNew,
               resolveSubGame,
               resolveSubGame,
               Keys.None,
               null,
               CommandVisibility.ContextMenu,
               this);


            string unresolveSubGame = "Unresolve SubGame".Localize();
            CommandService.RegisterCommand(
               Command.Unresolve,
              StandardMenu.File,
              StandardCommandGroup.FileNew,
               unresolveSubGame,
               unresolveSubGame,
               Keys.None,
               null,
               CommandVisibility.ContextMenu,
               this);

                    
            // on initialization, register our tree control with the hosting service
            m_controlHostService.RegisterControl(
                Control,
                new ControlInfo(
                    "Project Lister".Localize(),
                    "Lists objects in the current document".Localize(),
                    StandardControlGroup.Left),
                this);

            string ext = m_gameEditor.Info.Extensions[0];
            m_fileFilter = string.Format("SubGame (*{0})|*{0}", ext);

            if (m_scriptingService != null)
                m_scriptingService.SetVariable("projLister", this);
        }

        #endregion

        #region IControlHostClient Members

        /// <summary>
        /// Notifies the client that its Control has been activated. Activation occurs when
        /// the control gets focus, or a parent "host" control gets focus.</summary>
        /// <param name="control">Client Control that was activated</param>
        void IControlHostClient.Activate(Control control)
        {
            if (TreeView != null)
                m_contextRegistry.ActiveContext = TreeView;
        }

        /// <summary>
        /// Notifies the client that its Control has been deactivated. Deactivation occurs when
        /// another control or "host" control gets focus.</summary>
        /// <param name="control">Client Control that was deactivated</param>
        void IControlHostClient.Deactivate(Control control)
        {
        }

        /// <summary>
        /// Requests permission to close the client's Control.</summary>
        /// <param name="control">Client control to be closed</param>
        /// <returns>true if the control can close, or false to cancel.</returns>
        bool IControlHostClient.Close(Control control)
        {
            return true;
        }

        #endregion

        #region IContextMenuCommandProvider Members

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            ICommandClient cmdclient = (ICommandClient)this;
            
            if (context == this.TreeView)
            {
                if (Adapters.Is<IGame>(target) || Adapters.Is<GameReference>(target) || Adapters.Is<IResolveable>(target))
                {
                    foreach (Command command in Enum.GetValues(typeof(Command)))
                    {
                        if (cmdclient.CanDoCommand(command))
                        {
                            yield return command;
                        }
                    }
                }

                    // Some nodes can provide extra context menu commands
                    // Nodes that can will have an adapter derived from 
                    // IContextMenuCommandProvider
                var subProvider = target.As<IContextMenuCommandProvider>();
                if (subProvider != null)
                {
                    var subClient = target.Cast<ICommandClient>();
                    var registeredCommands = GetRegisteredCommands(subProvider);
                    foreach (var command in registeredCommands.m_commands)
                        if (subClient.CanDoCommand(command))
                            yield return command;
                }
            }
        }

        private class RegisteredSubProvider
        {
            public IEnumerable<object> m_commands;
            public RegisteredSubProvider(
                IContextMenuCommandProvider subProvider, 
                ICommandService commandService,
                ICommandClient client)
            {
                m_commands = subProvider.GetCommands(null, null);
                foreach (var c in m_commands)
                {
                    var description = GetAnnotatedDescription(c).Localize();
                    commandService.RegisterCommand(
                        c,
                        StandardMenu.File,
                        CustomCommandGroups.NodeSpecific,
                        description,
                        description,
                        Sce.Atf.Input.Keys.None,
                        null,
                        CommandVisibility.ContextMenu,
                        client);
                }
            }

            enum CustomCommandGroups { NodeSpecific }

            private static string GetAnnotatedDescription(object obj)
            {
                Type type = obj.GetType();
                if (!type.IsEnum)
                    throw new ArgumentException("EnumerationValue must be of Enum type", "enumerationValue");

                    // 
                    //  Attempt to get a "Description" attribute attached
                    //  to this enum value
                    //
                var memberInfo = type.GetMember(obj.ToString());
                if (memberInfo != null && memberInfo.Length > 0)
                {
                    var attrs = memberInfo[0].GetCustomAttributes(
                        typeof(System.ComponentModel.DescriptionAttribute), false);

                    if (attrs != null && attrs.Length > 0)
                        return ((System.ComponentModel.DescriptionAttribute)attrs[0]).Description;
                }

                return obj.ToString();
            }
        }
        private Dictionary<Type, RegisteredSubProvider> m_subProviders = new Dictionary<Type, RegisteredSubProvider>();
        private RegisteredSubProvider GetRegisteredCommands(IContextMenuCommandProvider subProvider)
        {
                // note that our dictionary uses the type of "subProvider", not the instance
                // this is really important for 2 reasons:
                //      * we don't want to hold a reference to the instance of subProvider
                //      * multiple objects of the same type can share the same subProvider
                //
                // But it means that we're expecting subProvider.GetCommands to return the
                // same results for *any* instance of that type. In other words, that method
                // should act like a static method.
            RegisteredSubProvider result;
            if (m_subProviders.TryGetValue(subProvider.GetType(), out result))
                return result;

            result = new RegisteredSubProvider(subProvider, CommandService, this);
            m_subProviders.Add(subProvider.GetType(), result);
            return result;
        }

        #endregion

        #region ICommandClient Members

        bool ICommandClient.CanDoCommand(object commandTag)
        {
            var target = TreeControlAdapter.LastHit;

            if (!(commandTag is Command))
            {
                var targetClient = target.As<ICommandClient>();
                if (targetClient != null)
                    return targetClient.CanDoCommand(commandTag);
                return false;
            }

            bool cando = false;
            switch ((Command)commandTag)
            {
                case Command.CreateNewSubGame:
                case Command.AddSubGame:
                    {
                        IGame game = target.As<IGame>();
                        if (game == null)
                        {
                            GameReference gameRef = target.As<GameReference>();
                            game = (gameRef != null) ? gameRef.Target : null;
                        }
                        cando = game != null;
                    }
                    break;
                case Command.Exclude:
                    cando = target.Is<GameReference>();
                    break;

                case Command.Resolve:
                    {
                        GameReference gameRef = target.As<GameReference>();
                        cando = gameRef != null && gameRef.Target == null;

                        if (!cando)
                        {
                            var resolveable = target.As<IResolveable>();
                            cando = resolveable != null && !resolveable.IsResolved();
                        }
                    }                    
                    break;

                case Command.Unresolve:
                    {
                        GameReference gameRef = target.As<GameReference>();
                        cando = gameRef != null && gameRef.Target != null;

                        if (!cando)
                        {
                            var resolveable = target.As<IResolveable>();
                            cando = resolveable != null && resolveable.IsResolved();
                        }
                    }                    
                    break;

                //<<XLE
                case Command.SetupWorldPlacements:
                    {
                        IGame game = target.As<IGame>();
                        cando = game != null;
                    }
                    break;
                //XLE>>
            }
            return cando;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            var target = TreeControlAdapter.LastHit;

                // If the command isn't one of our immediate commands, it
                // might belong to one of our sub providers.
                // Try to cast the targetted node to a command client and
                // execute from there
            if (!(commandTag is Command))
            {
                var targetClient = target.As<ICommandClient>();
                if (targetClient != null)
                    targetClient.DoCommand(commandTag);
                return;
            }

            IDocument gameDocument = null;
            string filePath = null;

            IGame game = target.As<IGame>();
            if (game == null)
            {
                GameReference gameRef = target.As<GameReference>();
                if (gameRef != null)
                    game = gameRef.Target;
            }

            if (game != null)
                gameDocument = game.As<IDocument>();

            switch ((Command)commandTag)
            {
                case Command.CreateNewSubGame:
                    if (gameDocument != null)
                    {
                        filePath = Util.GetFilePath(m_fileFilter,
                            System.IO.Path.GetDirectoryName(gameDocument.Uri.LocalPath), true);
                        if (!string.IsNullOrEmpty(filePath))
                        {
                            try
                            {
                                if (!m_gameEditor.Info.IsCompatiblePath(filePath))
                                    throw new Exception("Incompatible file type " + filePath);

                                Uri ur = new Uri(filePath);
                                if (m_gameDocumentRegistry.FindDocument(ur) != null)
                                    throw new Exception(filePath + " is already open");
                                GameDocument subGame = GameDocument.OpenOrCreate(ur, m_schemaLoader);
                                subGame.Dirty = true;
                                GameReference gameRef = GameReference.CreateNew(subGame);
                                IHierarchical parent = game.As<IHierarchical>();
                                parent.AddChild(gameRef);
                                // because we performing this operation outside of TransactionContext
                                // we must set Document Dirty flag.
                                gameDocument.Dirty = true;

                            }
                            catch (Exception ex)
                            {
                                MessageBox.Show(m_mainWindow.DialogOwner, ex.Message);
                            }
                        }
                    }
                    break;

                case Command.AddSubGame:
                    if (gameDocument != null)
                    {
                        filePath = Util.GetFilePath(m_fileFilter,
                            System.IO.Path.GetDirectoryName(gameDocument.Uri.LocalPath),
                            false);

                        if (!string.IsNullOrEmpty(filePath))
                        {
                            try
                            {
                                if (!m_gameEditor.Info.IsCompatiblePath(filePath))
                                    throw new Exception("Incompatible file type " + filePath);

                                Uri ur = new Uri(filePath);
                                if (m_gameDocumentRegistry.FindDocument(ur) != null)
                                    throw new Exception(filePath + " is already open");

                                GameReference gameRef = GameReference.CreateNew(ur);
                                gameRef.Resolve();
                                IHierarchical parent = game.As<IHierarchical>();
                                parent.AddChild(gameRef);

                                // because we performing this operation outside of TransactionContext
                                // we must set Document Dirty flag.
                                gameDocument.Dirty = true;
                                RefreshLayerContext();
                            }
                            catch (Exception ex)
                            {
                                MessageBox.Show(m_mainWindow.DialogOwner, ex.Message);
                            }
                        }
                    }
                    break;
                case Command.Exclude:
                    {
                        GameReference gameRef = target.As<GameReference>();
                        if (gameRef == null) { break; }

                        gameDocument = gameRef.DomNode.Parent.Cast<IDocument>();
                        GameDocument subDoc = gameRef.Target.Cast<GameDocument>();
                        
                        bool exclue = true;
                        bool save = false;
                        if (subDoc.Dirty)
                        {
                            string msg = "Save changes\r\n" + subDoc.Uri.LocalPath;
                            DialogResult dlgResult =
                                MessageBox.Show(m_mainWindow.DialogOwner, msg, m_mainWindow.Text
                                , MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);

                            save = dlgResult == DialogResult.Yes;
                            exclue = dlgResult != DialogResult.Cancel;
                        }
                        
                        if (save)
                            subDoc.Save(subDoc.Uri, m_schemaLoader);

                        if (exclue)
                        {
                            gameRef.DomNode.RemoveFromParent();
                            // because we performing this operation outside of TransactionContext
                            // we must set Document Dirty flag.                        
                            gameDocument.Dirty = true;
                            UpdateGameObjectReferences();
                            RefreshLayerContext();
                        }
                    }
                    break;
                case Command.Resolve:
                    {
                        bool madeChange = false;
                        GameReference gameRef = target.As<GameReference>();
                        if (gameRef != null)
                        {
                            gameRef.Resolve();
                            madeChange = true;
                        }
                        else
                        {
                            var resolveable = target.As<IResolveable>();
                            if (resolveable != null && !resolveable.IsResolved())
                            {
                                resolveable.Resolve();
                                madeChange = true;
                            }
                        }

                        if (madeChange)
                        {
                            TreeControlAdapter.Refresh(target);
                            RefreshLayerContext();
                        }
                    }
                    break;
                case Command.Unresolve:
                    {
                        try
                        {
                            GameReference gameRef = target.As<GameReference>();
                            if (gameRef!=null)
                            {
                                GameDocument subDoc = gameRef.Target.Cast<GameDocument>();
                                bool unresolve = true;
                                bool save = false;
                                if (subDoc.Dirty)
                                {
                                    string msg = "Save changes\r\n" + subDoc.Uri.LocalPath;
                                    DialogResult dlgResult =
                                        MessageBox.Show(m_mainWindow.DialogOwner, msg, m_mainWindow.Text
                                        , MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);

                                    save = dlgResult == DialogResult.Yes;
                                    unresolve = dlgResult != DialogResult.Cancel;
                                }
                                //cando = gameRef != null && gameRef.Target != null;
                                if (save)
                                    subDoc.Save(subDoc.Uri, m_schemaLoader);
                                if (unresolve)
                                {
                                    gameRef.Unresolve();
                                    UpdateGameObjectReferences();
                                    RefreshLayerContext();

                                }
                                TreeControlAdapter.Refresh(gameRef);
                            }
                            else
                            {
                                var resolveable = target.As<IResolveable>();
                                if (resolveable!=null && resolveable.IsResolved())
                                {
                                    resolveable.Unresolve();
                                    RefreshLayerContext();
                                    TreeControlAdapter.Refresh(target);
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            MessageBox.Show(m_mainWindow.DialogOwner, ex.Message);
                        }                             
                    }
                    break;

                //<<XLE
                case Command.SetupWorldPlacements:
                    if (game != null)
                    {
                        var newDoc = PlacementsFolder.CreateNew();
                        IHierarchical parent = game.As<IHierarchical>();
                        parent.AddChild(newDoc);
                        // because we performing this operation outside of TransactionContext
                        // we must set Document Dirty flag.
                        gameDocument.Dirty = true;
                    }
                    break;
                //XLE>>

                default:
                    throw new ArgumentOutOfRangeException("commandTag");
            }
            m_designView.InvalidateViews();
            Refresh();
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        {
            var target = TreeControlAdapter.LastHit;

            if (!(commandTag is Command))
            {
                var targetClient = target.As<ICommandClient>();
                if (targetClient != null)
                    targetClient.UpdateCommand(commandTag, commandState);
                return;
            }
        }

        #endregion

        protected override void Configure(out Sce.Atf.Controls.TreeControl treeControl,
                                          out TreeControlAdapter treeControlAdapter)
        {
            base.Configure(out treeControl, out treeControlAdapter);
            treeControl.ShowRoot = true;
            treeControl.AllowDrop = true;
        }

        /// <summary>
        /// Raises the LastHitChanged event and performs custom processing</summary>
        /// <param name="e">Event args</param>
        protected override void OnLastHitChanged(EventArgs e)
        {            
            // forward "last hit" information to the GameContext which needs to know
            //  where to insert objects during copy/paste and drag/drop. The base tracks
            //  the last clicked and last dragged over tree objects.
            GameContext context = Adapters.As<GameContext>(TreeView);
            if (context != null)
                context.SetActiveItem(LastHit);

            base.OnLastHitChanged(e);
        }

        private void ContextRegistry_ActiveContextChanged(object sender, EventArgs e)
        {
            IGameContext game = m_contextRegistry.GetActiveContext<IGameContext>();
            if (game == null)
            {
                TreeView = null;
            }
            else
            {
                ITreeView treeView = new FilteredTreeView((ITreeView)game, DefaultFilter);

                // If it's different, switch to it
                if (!FilteredTreeView.Equals(TreeView, treeView))
                {
                    TreeView = treeView;
                    UpdateFiltering(this, EventArgs.Empty);
                }
            }

            if (m_validationContext != null)
            {
                m_validationContext.Ended -= ValidationContext_Ended;
            }
            m_validationContext = (IValidationContext)game;
            if (m_validationContext != null)
            {
                m_validationContext.Ended += ValidationContext_Ended;
            }
        }

        private void ValidationContext_Ended(object sender, EventArgs e)
        {
            Refresh();
        }

        private void RefreshLayerContext()
        {
            var layerContext = m_gameDocumentRegistry.MasterDocument.As<LayeringContext>();
            if (layerContext != null)
                layerContext.RefreshRoot();
        }
        /// <summary>
        /// Unresolve all the GameObjectReferences, 
        /// if the target object is belong to the removed documents</summary>
        private void UpdateGameObjectReferences()
        {
            
            // Refresh LayerListers, after the following subgame operations.
            // adding 
            // unresolving
            // excluding  
            // for all Layer Lister need to be refreshed.
            
            foreach (var subDoc in m_gameDocumentRegistry.Documents)
            {
                var rootNode = subDoc.Cast<DomNode>();
                foreach (DomNode childNode in rootNode.Subtree)
                {
                    var gameObjectReference = childNode.As<GameObjectReference>();
                    if (gameObjectReference == null) continue;
                    var targetNode = Adapters.As<DomNode>(gameObjectReference.Target);
                    if(targetNode == null) continue;
                    var targetDoc = targetNode.GetRoot().As<IGameDocument>();
                    if(!m_gameDocumentRegistry.Contains(targetDoc))
                        gameObjectReference.UnResolve();
                }
            }
        }

        private readonly IControlHostService m_controlHostService;
        private readonly IContextRegistry m_contextRegistry;
                
        [Import(AllowDefault = false)]
        private IDesignView m_designView = null;

        [Import(AllowDefault = false)]
        private GameEditor m_gameEditor = null;

        [Import(AllowDefault = false)]
        private IMainWindow m_mainWindow = null;

        [Import(AllowDefault = false)]
        private SchemaLoader m_schemaLoader = null;

        [Import(AllowDefault = false)]
        private IGameDocumentRegistry m_gameDocumentRegistry = null;

        [Import(AllowDefault = true)]
        private ScriptingService m_scriptingService = null;

        private IValidationContext m_validationContext;

        private string m_fileFilter;
        private enum Command
        {
            CreateNewSubGame,
            AddSubGame,  
            Exclude,
            Resolve,
            Unresolve,

            //<<XLE
            SetupWorldPlacements
            //XLE>>
        }
    }
}
