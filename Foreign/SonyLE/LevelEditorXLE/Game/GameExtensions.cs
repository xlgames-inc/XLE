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
using LevelEditorXLE.Extensions;

namespace LevelEditorXLE.Game
{
    class GameExtensions : DomNodeAdapter, IHierarchical, ICommandClient, IContextMenuCommandProvider
    {
        #region IHierachical
        public bool CanAddChild(object child)
        {
            var domNode = child as DomNode;
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (type == Schema.placementsFolderType.Type) return true;
                    if (type == Schema.envSettingsFolderType.Type) return true;
                }
            }

            return  PlacementsFolder.CanAddChild(child)
                |   EnvSettingsFolder.CanAddChild(child);
        }
        public bool AddChild(object child)
        {
            if (EnvSettingsFolder.AddChild(child))
                return true;

            if (PlacementsFolder.AddChild(child))
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

                    if (domNode.Type == Schema.envSettingsFolderType.Type)
                    {
                        SetChild(Schema.xleGameType.environmentChild, domNode);
                        return true;
                    }
                }
            }

            return false;
        }
        #endregion

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

            set { SetChild(Schema.xleGameType.placementsChild, value); }
        }

        public Terrain.XLETerrainGob Terrain
        {
            get { return DomNode.GetChild(Schema.xleGameType.terrainChild).As<Terrain.XLETerrainGob>(); }
        }

        public string ExportDirectory
        {
            get { return GetAttribute<string>(Schema.xleGameType.ExportDirectoryAttribute); }
            set { SetAttribute(Schema.xleGameType.ExportDirectoryAttribute, value); }
        }

        public GUILayer.EditorSceneManager SceneManager
        {
            get { return XLEBridgeUtils.Utils.GlobalSceneManager; }
        }

        public GUILayer.TerrainManipulatorContext TerrainManipulatorContext
        {
            get { return m_terrainManipulatorContext; }
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
                    case Command.CreateTriMeshMarker:
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
                            var newNode = LevelEditorXLE.Terrain.XLETerrainGob.CreateWithConfigure();
                            if (newNode != null)
                                DomNode.SetChild(Schema.xleGameType.terrainChild, newNode);
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

                case Command.CreateTriMeshMarker:
                    {
                        this.As<IGame>().RootGameObjectFolder.As<IHierarchical>().AddChild(Markers.TriMeshMarker.Create());
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
            CreateEnvironmentSetting,
            [Description("Add tri mesh marker")]
            CreateTriMeshMarker
        }

        IEnumerable<object> IContextMenuCommandProvider.GetCommands(object context, object target)
        {
            foreach (Command command in System.Enum.GetValues(typeof(Command)))
                yield return command;
        }
        #endregion

        private GUILayer.TerrainManipulatorContext m_terrainManipulatorContext = new GUILayer.TerrainManipulatorContext();
    }

    class XLEGameObjectsFolder : DomNodeAdapter, IExportable
    {
        #region IExportable
        public string ExportTarget
        {
            get 
            {
                var parent = DomNode.GetRoot().As<GameExtensions>();
                if (parent != null) return parent.ExportDirectory + "GameObjects.txt";
                return "finals/GameObjects.txt"; 
            }
        }

        public string ExportCategory
        {
            get { return "Game Objects"; }
        }

        public IEnumerable<PendingExport> BuildPendingExports()
        {
            var result = new List<PendingExport>();
            result.Add(new PendingExport(ExportTarget, this.GetSceneManager().ExportGameObjects(0)));
            return result;
        }
        #endregion
    }
}
