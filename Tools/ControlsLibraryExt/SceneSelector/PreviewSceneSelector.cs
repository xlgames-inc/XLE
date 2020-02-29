using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using Aga.Controls.Tree;

namespace ControlsLibraryExt.SceneSelector
{
    public partial class PreviewSceneSelector : UserControl
    {
        public PreviewSceneSelector()
        {
            InitializeComponent();

            this.treeView1.Model = new TreeModel(GUILayer.Utils.EnumeratePreviewScenes());
            this.treeView1.SelectionChanged += TreeView1_SelectionChanged;
        }

        private void TreeView1_SelectionChanged(object sender, EventArgs e)
        {
            if (this.treeView1.SelectedNode != null)
            {
                var item = this.treeView1.SelectedNode.Tag as TreeModel.Item;
                if (item != null)
                {
                    OnSelectionChanged?.Invoke(this, new PreviewSceneSelectionChanged { PreviewScene = item.FullPath });
                }
            }
        }

        public class PreviewSceneSelectionChanged { public string PreviewScene; }
        public delegate void SampleEventHandler(object sender, PreviewSceneSelectionChanged e);
        public event SampleEventHandler OnSelectionChanged;

        internal class TreeModel : ITreeModel
        {
            internal class Item
            {
                public string Label;
                public string FullPath;
            }

            internal class Folder
            {
                public string Label;
                public List<Folder> ChildFolders = new List<Folder>();
                public List<Item> ChildItems = new List<Item>();
            }

            public System.Collections.IEnumerable GetChildren(TreePath treePath)
            {
                Folder fIteration = _baseFolder;
                if(!treePath.IsEmpty())
                {
                    fIteration = (Folder)treePath.LastNode;
                }

                foreach (var i in fIteration.ChildFolders)
                    yield return i;
                foreach (var i in fIteration.ChildItems)
                    yield return i;
            }

            public bool IsLeaf(TreePath treePath)
            {
                if (treePath.IsEmpty())
                    return true;

                return treePath.LastNode.GetType() != typeof(Folder);
            }

            public event EventHandler<TreeModelEventArgs> NodesChanged;
            public event EventHandler<TreeModelEventArgs> NodesInserted;
            public event EventHandler<TreeModelEventArgs> NodesRemoved;
            public event EventHandler<TreePathEventArgs> StructureChanged;

            public TreeModel(IEnumerable<string> enumeratedScenes)
            {
                List<String> sortedEnumeratedScenes = enumeratedScenes.ToList();
                sortedEnumeratedScenes.Sort();

                foreach(var i in sortedEnumeratedScenes)
                {
                    Folder fIteration = _baseFolder;
                    int iBegin = 0;
                    while(iBegin < i.Length) {
                        var iEnd = i.IndexOf('/', iBegin);
                        if (iEnd == -1)
                        {
                            var label = i.Substring(iBegin);
                            if (fIteration.ChildItems.Find(p => p.Label == label) == null)
                                fIteration.ChildItems.Add(new Item { Label = label, FullPath = i });
                            iBegin = i.Length;
                        }
                        else
                        {
                            var label = i.Substring(iBegin, iEnd - iBegin);
                            var item = fIteration.ChildFolders.Find(p => p.Label == label);
                            if (item == null)
                            {
                                item = new Folder { Label = label };
                                fIteration.ChildFolders.Add(item);
                            }
                            fIteration = item;
                            iBegin = iEnd + 1;
                        }
                    }
                }
            }

            private Folder _baseFolder = new Folder();
        }
    }
}
