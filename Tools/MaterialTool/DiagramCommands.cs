// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Input;
using Sce.Atf.Controls.Adaptable;

namespace MaterialTool
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class DiagramCommands : ICommandClient, IInitializable
    {
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            switch ((Command)commandTag)
            {
                case Command.DiagramSettings:
                case Command.ShowInPreviewer:
                    return _contextRegistry.GetActiveContext<DiagramEditingContext>() != null;
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.DiagramSettings:
                        {
                            var context = _contextRegistry.GetActiveContext<DiagramEditingContext>();
                            if (context != null)
                            {
                                (new DiagramSettings(context.Document)).ShowDialog();
                            }
                        }
                        break;

                    case Command.ShowInPreviewer:
                        {
                            var context = _contextRegistry.GetActiveContext<DiagramEditingContext>();
                            if (context != null && context.Document.NodeGraphFile.SubGraphs.Count != 0)
                            {
                                ControlsLibraryExt.ModelView.PreviewerContext previewerContext = null;
                                Controls.ActualizationMessagesWindow msgsWindow = null;

                                foreach (var control in _controlHostService.Controls)
                                {
                                    var adaptableControl = control.Control as AdaptableControl;
                                    if (adaptableControl != null) {
                                        var c = adaptableControl.ContextAs<ControlsLibraryExt.ModelView.PreviewerContext>();
                                        if (c != null) previewerContext = c;
                                    }

                                    var mw = control.Control as Controls.ActualizationMessagesWindow;
                                    if (mw != null)
                                        msgsWindow = mw;
                                }

                                if (previewerContext != null)
                                {
                                    var actualizationMsgs = new ShaderPatcherLayer.MessageRelayWrapper();
                                    previewerContext.TechniqueOverrides =
                                        _previewManager.MakeTechniqueDelegate(
                                            context.Document.NodeGraphFile,
                                            context.Document.NodeGraphFile.SubGraphs.First().Key,
                                            actualizationMsgs);

                                    if (msgsWindow!=null)
                                        msgsWindow.SetContext(actualizationMsgs);
                                }
                            }
                        }
                        break;
                }
            }
        }

        public void UpdateCommand(object commandTag, CommandState state) {}

        public virtual void Initialize()
        {
            _commandService.RegisterCommand(
                new CommandInfo(
                    Command.DiagramSettings,
                    StandardMenu.Edit,
                    "Diagram",
                    "Diagram Settings...".Localize(),
                    "Choose settings for the shader diagram".Localize(),
                    Keys.None,
                    null,
                    CommandVisibility.Menu),
                this);
            _commandService.RegisterCommand(
                new CommandInfo(
                    Command.ShowInPreviewer,
                    StandardMenu.Edit,
                    "ShowInPreviewer",
                    "Show In Previewer".Localize(),
                    "Show this shader diagram in the previewer".Localize(),
                    Keys.None,
                    null,
                    CommandVisibility.Menu),
                this);
        }

        private enum Command
        {
            DiagramSettings,
            ShowInPreviewer
        }

        [Import(AllowDefault = false)]
        private ICommandService _commandService;

        [Import(AllowDefault = false)]
        private IContextRegistry _contextRegistry;

        [Import(AllowDefault = false)]
        private IControlHostService _controlHostService;

        [Import]
        private ShaderPatcherLayer.IPreviewBuilder _previewManager;
    }
}
