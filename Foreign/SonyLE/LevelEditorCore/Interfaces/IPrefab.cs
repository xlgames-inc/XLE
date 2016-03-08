//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.Collections.Generic;

using Sce.Atf;

namespace LevelEditorCore
{
    /// <summary>
    /// Prefab interface</summary>
    public interface IPrefab : IResource
    {
        IEnumerable<object> GameObjects { get; }
        string Name { get; }
    }

    public interface IPrefabInstance
    {
        void Resolve(UniqueNamer namer);
    }
}
