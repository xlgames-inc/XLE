using System;
using System.Collections.Generic;

namespace LevelEditorXLE
{
    public class PendingExport
    {
        public Uri TargetFile { get; set; }
        public GUILayer.EditorSceneManager.PendingExport Export { get; set; }

        public PendingExport(Uri targetFile, GUILayer.EditorSceneManager.PendingExport pendingExport)
        {
            TargetFile = targetFile;
            Export = pendingExport;
        }
    }
    
    public interface IExportable
    {
        string ExportCategory { get; }
        IEnumerable<PendingExport> BuildPendingExports();
    }
}
