// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)


using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Aga.Controls.Tree;

namespace ControlsLibraryExt
{
    public interface ISerializableDocument
    {
        byte[] Serialize();
    }

    class ModifiedDocumentPendingSaveList : GUILayer.PendingSaveList
    {
        public Dictionary<Sce.Atf.IDocument, uint> DocToId = new Dictionary<Sce.Atf.IDocument, uint>();
        public ISet<string> DocTypes = new HashSet<string>();

        public ModifiedDocumentPendingSaveList(
            Sce.Atf.Applications.IDocumentRegistry docRegistry,
            Sce.Atf.Applications.IDocumentService docService)
        {
            _docService = docService;

            uint nextId = 1;
            foreach (var doc in docRegistry.Documents)
            {
                if (!doc.Dirty)
                    continue;
                var serializableDoc = doc as ISerializableDocument;
                if (serializableDoc == null)
                    continue;

                var entry = new Entry(
                    doc.Uri.OriginalString,
                    System.IO.File.ReadAllBytes(doc.Uri.LocalPath),
                    serializableDoc.Serialize());

                Add(0, nextId, entry);
                DocToId.Add(doc, nextId);
                ++nextId;

                DocTypes.Add(doc.Type);
            }
        }

        public override CommitResult Commit()
        {
            var result = new CommitResult();

            foreach (var doc in DocToId)
            {
                var entry = GetEntry(0, doc.Value);
                if (entry == null) continue;

                // "abandoning" of changes not supported for document types
                System.Diagnostics.Debug.Assert(entry._action != Action.Abandon);

                if (entry._action != Action.Save)
                    continue;

                _docService.Save(doc.Key);
            }

            return result;
        }

        public static bool HasModifiedDocuments(Sce.Atf.Applications.IDocumentRegistry docRegistry)
        {
            foreach (var doc in docRegistry.Documents)
                if (doc.Dirty && (doc as ISerializableDocument) != null)
                    return true;
            return false;
        }

        private Sce.Atf.Applications.IDocumentService _docService;
    }

    class ModifiedDocumentList : ITreeModel
    {
        public IEnumerable GetChildren(TreePath treePath)
        {
            if (treePath.IsEmpty())
            {
                foreach (var type in _pendingSaveList.DocTypes)
                {
                    var typeItem = new GUILayer.AssetTypeItem(type, null);
                    foreach (var doc in _docRegistry.Documents)
                        if (doc.Dirty && doc.Type.Equals(type))
                        {
                            if (_pendingSaveList.DocToId.TryGetValue(doc, out uint docId))
                                typeItem._children.Add(new GUILayer.AssetItem(
                                    doc.Uri.OriginalString,
                                    _pendingSaveList.GetEntry(0, docId)));
                        }

                    yield return typeItem;
                }
            }
            else
            {
                var lastItem = treePath.LastNode as GUILayer.AssetTypeItem;
                if (lastItem != null)
                {
                    foreach (var item in lastItem._children)
                        yield return item;
                }
            }
        }
        
        public bool IsLeaf(TreePath treePath)
        {
            return !(treePath.LastNode is GUILayer.AssetTypeItem);
        }

        public event EventHandler<TreeModelEventArgs> NodesChanged;
        public event EventHandler<TreeModelEventArgs> NodesInserted;
        public event EventHandler<TreeModelEventArgs> NodesRemoved;
        public event EventHandler<TreePathEventArgs> StructureChanged;

        public ModifiedDocumentList(
            Sce.Atf.Applications.IDocumentRegistry docRegistry, 
            ModifiedDocumentPendingSaveList pendingSaveList) 
        {
            _docRegistry = docRegistry;
            _pendingSaveList = pendingSaveList;
        }

        private Sce.Atf.Applications.IDocumentRegistry _docRegistry;
        private ModifiedDocumentPendingSaveList _pendingSaveList;
    }

    class MergedDocumentList : ITreeModel
    {
        public IEnumerable GetChildren(TreePath treePath)
        {
            if (treePath.IsEmpty())
            {
                foreach(var childModel in _children)
                {
                    var childResult = childModel.GetChildren(treePath);
                    foreach (var c in childResult)
                    {
                        _rootsToSource.Add(c, childModel);
                        yield return c;
                    }
                }
            }
            else
            {
                if (_rootsToSource.TryGetValue(treePath.FirstNode, out ITreeModel childModel))
                {
                    var childResult = childModel.GetChildren(treePath);
                    foreach (var c in childResult)
                        yield return c;
                }
            }
        }

        public bool IsLeaf(TreePath treePath)
        {
            if (treePath.IsEmpty()) return false;
            if (_rootsToSource.TryGetValue(treePath.FirstNode, out ITreeModel childModel))
                return childModel.IsLeaf(treePath);
            return true;
        }

        public event EventHandler<TreeModelEventArgs> NodesChanged;
        public event EventHandler<TreeModelEventArgs> NodesInserted;
        public event EventHandler<TreeModelEventArgs> NodesRemoved;
        public event EventHandler<TreePathEventArgs> StructureChanged;

        public MergedDocumentList(IEnumerable<ITreeModel> children)
        {
            _children = children.ToList();
            // todo -- these callbacks are never detached
            foreach (var c in _children)
            {
                c.NodesChanged += (object sender, TreeModelEventArgs args) => NodesChanged?.Invoke(sender, args);
                c.NodesInserted += (object sender, TreeModelEventArgs args) => NodesInserted?.Invoke(sender, args);
                c.NodesRemoved += (object sender, TreeModelEventArgs args) => NodesRemoved?.Invoke(sender, args);
                c.StructureChanged += (object sender, TreePathEventArgs args) => StructureChanged?.Invoke(sender, args);
            }
        }

        private List<ITreeModel> _children = new List<ITreeModel>();
        private Dictionary<object, ITreeModel> _rootsToSource = new Dictionary<object, ITreeModel>();
    }
}

