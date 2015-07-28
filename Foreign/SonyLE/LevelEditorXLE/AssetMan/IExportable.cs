using System;
using System.Collections.Generic;

namespace LevelEditorXLE
{
    using PendingExports = IEnumerable<Tuple<string, GUILayer.EditorSceneManager.PendingExport>>;
    
    public interface IExportable
    {
        string ExportCategory { get; }
        PendingExports BuildPendingExports();
    }
}
