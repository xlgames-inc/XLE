using System;
using System.Collections.Generic;

namespace LevelEditorXLE
{
    public interface IExportable
    {
        string ExportTarget { get; set; }
        string ExportCategory { get; }
        GUILayer.EditorSceneManager.ExportResult PerformExport(string destinationFile);
        GUILayer.EditorSceneManager.ExportPreview PreviewExport();
    }
}
