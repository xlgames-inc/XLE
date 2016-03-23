// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Adaptation;
using Sce.Atf.Dom;

using LevelEditorCore;

#pragma warning disable 0649 // Field '...' is never assigned to, and will always have its default value null

namespace LevelEditorXLE
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class Commands : ICommandClient, IInitializable
    {
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            switch ((Command)commandTag)
            {
                case Command.ExportToGame:
                    return true;
                case Command.ExecuteEnvironmentSample:
                    return true;
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;
            
            switch ((Command)commandTag)
            {
                case Command.ExportToGame:
                    PerformExportToGame();
                    break;
                case Command.ExecuteEnvironmentSample:
                    PerformExecuteEnvironmentSample();
                    break;
            }
        }

        public void UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state) {}

        public virtual void Initialize()
        {
            m_commandService.RegisterCommand(
                Command.ExportToGame,
                StandardMenu.File,
                "Assets",
                "Export to Game...".Localize(),
                "Export all changed files to game format".Localize(),
                Keys.None,
                LevelEditorCore.Resources.CubesImage,
                CommandVisibility.Menu,
                this);

            m_commandService.RegisterCommand(
                Command.ExecuteEnvironmentSample,
                StandardMenu.File,
                "Assets",
                "Execute Environment Sample".Localize(),
                "Run the environment sample with the active level".Localize(),
                Keys.None,
                LevelEditorCore.Resources.CubesImage,
                CommandVisibility.Menu,
                this);
        }

        private enum Command
        {
            ExportToGame,
            ExecuteEnvironmentSample
        }

        private void PerformExportToGame()
        {
            // Export operations are performed on nodes in the dom hierarchy
            // We need to search through our nodes to find those that implement
            // IExportable.
            //
            // Sometimes these nodes might be able to provide previews of the
            // export operation (though maybe only when exporting simple text
            // format files).

                // Awkward to get the root node from here... Technically, the context
                // should be an IGame. But we can cast that directly to a DomNodeAdapter.
            var rootNode = m_designView.ActiveView.DesignView.Context as DomNodeAdapter;
            if (rootNode==null) return;

            var queuedExports = new List<ControlsLibrary.ExportPreviewDialog.QueuedExport>();
            var exportNodes = new List<PendingExport>();
            foreach (var n in rootNode.DomNode.Subtree)     // LevelSubtree has a small bug "foreach (var child in Children)" -> "foreach (var child in node.Children)"
            {
                var exportable = n.As<IExportable>();
                if (exportable == null) continue;

                var exports = exportable.BuildPendingExports();
                foreach (var preview in exports)
                {
                    var e = new ControlsLibrary.ExportPreviewDialog.QueuedExport
                        {
                            DoExport = true,
                            TargetFile = preview.TargetFile,
                            Category = exportable.ExportCategory
                        };

                    if (preview.Export._previewType == GUILayer.EditorSceneManager.PendingExport.Type.Text
                        || preview.Export._previewType == GUILayer.EditorSceneManager.PendingExport.Type.MetricsText)
                    {
                        e.TextPreview = preview.Export._preview;
                        var comparisonFile = e.TargetFile.LocalPath;
                        if (preview.Export._previewType == GUILayer.EditorSceneManager.PendingExport.Type.MetricsText)
                            comparisonFile += ".metrics";

                        try
                        {
                            e.ExistingText = System.IO.File.ReadAllText(comparisonFile);
                        }
                        catch
                        {
                            e.ExistingText = String.Format("<<Error while reading file {0}>>".Localize(), comparisonFile);
                        }
                    }
                    e.Messages = preview.Export._messages;
                    queuedExports.Add(e);
                    exportNodes.Add(preview);
                }
            }

            using (var dialog = new ControlsLibrary.ExportPreviewDialog())
            {
                dialog.QueuedExports = queuedExports;
                if (dialog.ShowDialog() == DialogResult.OK)
                {
                        // export now..
                        //  (for long export operations we could pop up a 
                        //  progress bar, or push this into a background thread)
                    for (int c = 0; c < queuedExports.Count; ++c)
                    {
                        if (queuedExports[c].DoExport)
                            exportNodes[c].Export.PerformExport(queuedExports[c].TargetFile);
                    }
                }
            } 
        }

        private void PerformExecuteEnvironmentSample()
        {
            var rootNode = m_designView.ActiveView.DesignView.Context as DomNodeAdapter;
            if (rootNode==null) return;

            var gameExt = rootNode.As<Game.GameExtensions>();
            if (gameExt==null) return;

            MessageBox.Show(
                "This will execute the environment sample, and load your level.\nYou should do threes things first.\n\n1) add a CharacterSpawn object for the player character (by dragging in from the Palette window).\n2) Add a sun, ambient settings and shadow settings.\n3) do a \"Export To Game\".\n\n"
                + "The sample will reload any subsequent exports.");

            var process = System.Diagnostics.Process.GetCurrentProcess(); // Or whatever method you are using
            string fullPath = process.MainModule.FileName;

            var envExe = new Uri(new Uri(fullPath), "environment.exe");

            var resService = Globals.MEFContainer.GetExportedValue<IXLEAssetService>();
            System.Diagnostics.Process.Start(
                envExe.LocalPath, 
                resService.AsAssetName(gameExt.ExportDirectory));
        }

        [Import(AllowDefault = false)]
        private ICommandService m_commandService;

        [Import(AllowDefault = false)]
        private IDesignView m_designView = null;
    }
}
