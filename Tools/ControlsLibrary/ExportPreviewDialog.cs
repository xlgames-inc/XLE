using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;
using Aga.Controls.Tree;

namespace ControlsLibrary
{
    public partial class ExportPreviewDialog : Form
    {
        public ExportPreviewDialog()
        {
            InitializeComponent();
        }

        public class QueuedExport
        {
            public bool DoExport { get; set; }
            public string TargetFile { get; set; }
            public string ExistingText { get; set; }
            public string TextPreview { get; set; }
            public string Category { get; set; }
            public string Preview { get; set; }
            public string Messages { get; set; }
        }

        public IEnumerable<QueuedExport> QueuedExports
        {
            set
            {
                _assetList.Model = new ExportTreeModel(value);
                _assetList.AutoSizeColumn(_labelColumn);
            }
        }

        internal class ExportTreeModel : ITreeModel
        {
            internal class Item
            {
                public string Label
                {
                    get { return Attached.TargetFile; }
                }
                public CheckState Enabled
                {
                    get { return Attached.DoExport ? CheckState.Checked : CheckState.Unchecked; }
                    set { Attached.DoExport = value == CheckState.Checked; }
                }
                public QueuedExport Attached;
            }

            internal class Category
            {
                public string Label;
                public CheckState Enabled
                {
                    get { return _enabled; }
                    set
                    {
                        _enabled = value;
                        foreach (var i in _items)
                            i.Enabled = _enabled;
                    }
                }
                public List<Item> _items = new List<Item>();
                private CheckState _enabled;
            }

            public System.Collections.IEnumerable GetChildren(TreePath treePath)
            {
                if (treePath.IsEmpty())
                {
                    var categories = new List<Category>();
                    foreach (var c in _queuedExports)
                    {
                        var cat = categories.Find(q => q.Label == c.Category);
                        if (cat == null)
                        {
                            cat = new Category { Label = c.Category, Enabled = CheckState.Checked };
                            categories.Add(cat);
                        }
                        cat._items.Add(new Item { Attached = c });
                    }
                    return categories;
                }
                else
                {
                    // find all of the items in the given category
                    var cat = treePath.LastNode as Category;
                    if (cat != null)
                    {
                        return cat._items;
                    }
                }
                return null;
            }

            public bool IsLeaf(TreePath treePath)
            {
                if (treePath.IsEmpty()) return false;
                if (treePath.LastNode is Item) return true;
                var cat = treePath.LastNode as Category;
                if (cat != null) return cat._items.Count == 0;
                return true;
            }

            public event EventHandler<TreeModelEventArgs> NodesChanged;
            public event EventHandler<TreeModelEventArgs> NodesInserted;
            public event EventHandler<TreeModelEventArgs> NodesRemoved;
            public event EventHandler<TreePathEventArgs> StructureChanged;

            public ExportTreeModel(IEnumerable<ExportPreviewDialog.QueuedExport> queuedExports)
            {
                _queuedExports = queuedExports;
            }

            private IEnumerable<ExportPreviewDialog.QueuedExport> _queuedExports;
        }

        private void OnTreeResize(object sender, EventArgs e)
        {
            _assetList.AutoSizeColumn(_labelColumn);
        }

        private void OnSelectionChange(object sender, EventArgs e)
        {
            var item = (_assetList.SelectedNode != null) ? (_assetList.SelectedNode.Tag as ExportTreeModel.Item) : null;
            if (item != null && item.Attached.TextPreview != null)
            {
                _compareWindow.Comparison = new Tuple<object, object>(
                    item.Attached.ExistingText, item.Attached.TextPreview);
            } 
            else 
            {
                _compareWindow.Comparison = null;
            }
        }
    }
}
