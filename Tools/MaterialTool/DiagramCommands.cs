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
                        var context = _contextRegistry.GetActiveContext<DiagramEditingContext>();
                        if (context != null)
                        {
                            (new DiagramSettings(context.UnderlyingDocument)).ShowDialog();
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
        }

        private enum Command
        {
            DiagramSettings
        }

        [Import(AllowDefault = false)]
        private ICommandService _commandService;

        [Import(AllowDefault = false)]
        private IContextRegistry _contextRegistry;
    }
}
