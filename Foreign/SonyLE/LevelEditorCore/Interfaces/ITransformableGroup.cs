//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.Collections.Generic;

namespace LevelEditorCore
{
    /// <summary>
    /// DomObjectInterface for game object groups</summary>
    public interface ITransformableGroup : ITransformable, IHierarchical
    {
        /// <summary>
        /// Gets the list of all child game objects</summary>
        /// <remarks>Sub-groups are included in this list as well</remarks>
        IList<ITransformable> Objects
        {
            get;
        }
    }
}
