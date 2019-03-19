//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using Sce.Atf;

namespace LevelEditorCore
{
    public interface IFileSystemResourceFolder : IOpaqueResourceFolder
    {

        /// <summary>
        /// Gets full path of this resource folder.</summary>
        string FullPath {get;}
    }
}
