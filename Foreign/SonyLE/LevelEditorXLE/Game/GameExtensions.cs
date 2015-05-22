// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.Collections.Generic;
using System.ComponentModel;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;
using LevelEditorXLE.Environment;
using LevelEditorXLE.Placements;

namespace LevelEditorXLE.Game
{
    class GameExtensions : DomNodeAdapter, IHierarchical, ICommandClient, IContextMenuCommandProvider
    {
        public bool CanAddChild(object child)
        {
            var domNode = child as DomNode;
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (type == Schema.placementsFolderType.Type) return true;
                    if (type == Schema.abstractPlacementObjectType.Type) return true;
                    if (type == Schema.envSettingsFolderType.Type) return true;
                }
            }
            if (EnvSettingsFolder.CanAddChild(child)) return true;

            return false;
        }

        public bool AddChild(object child)
        {
            if (EnvSettingsFolder.AddChild(child))
                return true;

            var domNode = child.As<DomNode>();
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (domNode.Type == Schema.placementsFolderType.Type)
                    {
                        SetChild(Schema.xleGameType.placementsChild, domNode);
                        return true;
                    }

                    if (type == Schema.abstractPlacementObjectType.Type)
                    {
                        // We need to look for the appropriate placement cell to put this placement in...
                        // If there are no placement cells, or if there isn't a cell that can contain this
                        // object, then we have to abort
                        var plc = domNode.As<ITransformable>();
                        var keyPoint = (plc != null) ? plc.Translation : new Sce.Atf.VectorMath.Vec3F(0.0f, 0.0f, 0.0f);

                        var placementsFolder = GetChild<PlacementsFolder>(Schema.xleGameType.placementsChild);
                        if (placementsFolder != null)
                        {
                            foreach (var cellRef in placementsFolder.Cells)
                            {
                                if (cellRef.IsResolved()
                                    && keyPoint.X >= cellRef.Mins.X && keyPoint.X < cellRef.Maxs.X
                                    && keyPoint.Y >= cellRef.Mins.Y && keyPoint.Y < cellRef.Maxs.Y
                                    && keyPoint.Z >= cellRef.Mins.Z && keyPoint.Z < cellRef.Maxs.Z
                                    && cellRef.Target.CanAddChild(domNode))
                                {
                                    return cellRef.Target.AddChild(domNode);
                                }
                            }
                        }
                    }

                    if (domNode.Type == Schema.envSettingsFolderType.Type)
                    {
                        SetChild(Schema.xleGameType.environmentChild, domNode);
                        return true;
                    }
                }
            }

            return false;
        }

        public XLEEnvSettingsFolder EnvSettingsFolder
        {
            get
            {
                var result = GetChild<XLEEnvSettingsFolder>(Schema.xleGameType.environmentChild);
                if (result == null)
                {
                    result = XLEEnvSettingsFolder.Create().Cast<XLEEnvSettingsFolder>();
                    SetChild(Schema.xleGameType.environmentChild, result);
                }
                return result;
            }
        }

        public Placements.PlacementsFolder PlacementsFolder
        {
            get
            {
                return GetChild<Placements.PlacementsFolder>(Schema.xleGameType.placementsChild);
            }
        }

        #region Context Menu Commands
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.CreateTerrain:
                        return DomNode.GetChild(Schema.xleGameType.terrainChild) == null;
                    case Command.CreatePlacementsFolder:
                        return DomNode.GetChild(Schema.xleGameType.placementsChild) == null;
                    case Command.CreateEnvironmentSetting:
                        return true;
                }
            }
            return false;
        }

        void ICommandClient.DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.CreateTerrain:
                    {
                        if (DomNode.GetChild(Schema.xleGameType.terrainChild) == null)
                        {
                            DomNode.SetChild(
                                Schema.xleGameType.terrainChild,
                                Terrain.XLETerrainGob.Create());
                        }
                        break;
                    }

                case Command.CreatePlacementsFolder:
                    {
                        if (DomNode.GetChild(Schema.xleGameType.placementsChild) == null)
                        {
                            var newNode = PlacementsFolder.CreateWithConfigure();
                            if (newNode != null)
                                DomNode.SetChild(Schema.xleGameType.placementsChild, newNode);
                        }
                        break;
                    }

                case Command.CreateEnvironmentSetting:
                    {
                        var envFolder = EnvSettingsFolder;
                        envFolder.AddChild(
                            XLEEnvSettings.Create(envFolder.GetNameForNewChild()));
                        break;
                    }
            }
        }

        void ICommandClient.UpdateCommand(object commandTag, CommandState commandState)
        { }

        private enum Command
        {
            [Description("Add Terrain")]
            CreateTerrain,
            [Description("Add Placements...")]
            CreatePlacementsFolder,
            [Description("Add New Environment Settings")]
            CreateEnvironmentSetting
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in System.Enum.GetValues(typeof(Command)))
                yield return command;
        }
        #endregion
    }
}
