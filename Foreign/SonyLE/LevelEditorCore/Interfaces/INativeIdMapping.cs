using System;

namespace LevelEditorCore
{
    public interface INativeIdMapping
    {
        Sce.Atf.Adaptation.IAdaptable GetAdapter(
            ulong nativeDocId, ulong nativeObjectId);
    }
}
