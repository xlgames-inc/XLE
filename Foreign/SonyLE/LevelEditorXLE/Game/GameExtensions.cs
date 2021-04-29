// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System.Collections.Generic;
using System.ComponentModel;
using System;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;
#if GUILAYER_SCENEENGINE
using LevelEditorXLE.Environment;
#endif
using LevelEditorXLE.Placements;
using LevelEditorXLE.Extensions;

namespace LevelEditorXLE.Game
{
    class GameExtensions : DomNodeAdapter, IHierarchical, ICommandClient, IContextMenuCommandProvider
    {
#region IHierachical
        public bool CanAddChild(object child)
        {
            if (child.Is<IGameObject>() || child.Is<IGameObjectFolder>()) return false;

            var domNode = child as DomNode;
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (type == Schema.placementsFolderType.Type) return true;
                    if (type == Schema.envSettingsFolderType.Type) return true;
                }
            }

#if GUILAYER_SCENEENGINE
            return  PlacementsFolder.CanAddChild(child)
                |   EnvSettingsFolder.CanAddChild(child);
#else
            return PlacementsFolder.CanAddChild(child);
#endif
        }
        public bool AddChild(object child)
        {
            if (child.Is<IGameObject>() || child.Is<IGameObjectFolder>()) return false;

#if GUILAYER_SCENEENGINE
            if (EnvSettingsFolder.AddChild(child))
                return true;
#endif

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

#if GUILAYER_SCENEENGINE
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
#endif

        public Placements.PlacementsFolder PlacementsFolder
        {
            get
            {
                return GetChild<Placements.PlacementsFolder>(Schema.xleGameType.placementsChild);
            }

            set { SetChild(Schema.xleGameType.placementsChild, value); }
        }

#if GUILAYER_SCENEENGINE
        public Terrain.XLETerrainGob Terrain
        {
            get { return DomNode.GetChild(Schema.xleGameType.terrainChild).As<Terrain.XLETerrainGob>(); }
        }
#endif
        public Uri ExportDirectory
        {
            get 
            { 
                var uri = GetAttribute<Uri>(Schema.xleGameType.ExportDirectoryAttribute);
                if (uri.IsAbsoluteUri)
                    return uri;
                // note -- we could check to see if this document is untitled here... But that requires using StandardFileCommands
                var res = this.As<IResource>();
                if (res != null && res.Uri != null)
                    return new Uri(res.Uri, uri.OriginalString);
                return new Uri(Utils.CurrentDirectoryAsUri(), uri.OriginalString);
            }
            set { SetAttribute(Schema.xleGameType.ExportDirectoryAttribute, value); }
        }

        public GUILayer.EditorSceneManager SceneManager
        {
            get { return XLEBridgeUtils.Utils.GlobalSceneManager; }
        }

#if GUILAYER_SCENEENGINE
        public GUILayer.TerrainManipulatorContext TerrainManipulatorContext
        {
            get { return m_terrainManipulatorContext; }
        }
#endif

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
#if GUILAYER_SCENEENGINE
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
#endif

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

#if GUILAYER_SCENEENGINE
                case Command.CreateEnvironmentSetting:
                    {
                        var envFolder = EnvSettingsFolder;
                        envFolder.AddChild(
                            XLEEnvSettings.Create(envFolder.GetNameForNewChild()));
                        break;
                    }
#endif

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

#if GUILAYER_SCENEENGINE
        private GUILayer.TerrainManipulatorContext m_terrainManipulatorContext = new GUILayer.TerrainManipulatorContext();
#endif
    }

    class XLEGameObjectsFolder : DomNodeAdapter, IExportable
    {
#region IExportable
        public Uri ExportTarget
        {
            get
            {
                var parent = DomNode.GetRoot().As<GameExtensions>();
                if (parent != null) return new Uri(parent.ExportDirectory, "GameObjects.txt");
                return new Uri("GameObjects.txt");
            }
        }

        public string ExportCategory
        {
            get { return "Game Objects"; }
        }

        public IEnumerable<PendingExport> BuildPendingExports()
        {
            var result = new List<PendingExport>();
            result.Add(
                new PendingExport(
                    ExportTarget, 
                    this.GetSceneManager().ExportGameObjects(0)));
            return result;
        }
#endregion
    }
}
