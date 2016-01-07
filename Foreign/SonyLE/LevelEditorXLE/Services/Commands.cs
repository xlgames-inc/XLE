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
using Sce.Atf.VectorMath;
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
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                switch ((Command)commandTag)
                {
                    case Command.ExportToGame:
                        PerformExportToGame();
                        break;
                }
            }
        }

        public void UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state)
        {
            // if (commandTag is Command)
            // {
            //     switch ((Command)commandTag)
            //     {
            //     }
            // }
        }

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
        }

        private enum Command
        {
            ExportToGame
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
                        var comparisonFile = e.TargetFile;
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

        [Import(AllowDefault = false)]
        private ICommandService m_commandService;

        [Import(AllowDefault = false)]
        private IDesignView m_designView = null;
    }
}
