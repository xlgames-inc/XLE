// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.ComponentModel.Composition;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace MaterialTool
{
    [Export(typeof(DiagramEditingContext))]
    [PartCreationPolicy(CreationPolicy.NonShared)]
    public class DiagramEditingContext :
        IEnumerableContext
        , IInstancingContext
        , IObservableContext
        , INamingContext
        , IColoringContext
        , ISelectionContext
        , ITreeListView
        , IItemView
        , NodeEditorCore.IEditingContext
    {
        private HyperGraph.IGraphSelection _selection;

        public HyperGraph.IGraphSelection DiagramSelection { get { return _selection; } }
        public NodeEditorCore.IDiagramDocument Document { get { return _containingDocument; } }
        public HyperGraph.IGraphModel ViewModel { get { return _containingDocument.ViewModel; } }
        public DiagramDocument ContainingDocument
        {
            set {
                if (_containingDocument != null)
                {
                    _containingDocument.ViewModel.NodeAdded -= model_NodeAdded;
                    _containingDocument.ViewModel.NodeRemoved -= model_NodeRemoved;
                    _containingDocument.ViewModel.ConnectionAdded -= model_ConnectionAdded;
                    _containingDocument.ViewModel.ConnectionRemoved -= model_ConnectionRemoved;
                }

                _containingDocument = value;

                if (_containingDocument != null)
                {
                    ViewModel.NodeAdded += model_NodeAdded;
                    ViewModel.NodeRemoved += model_NodeRemoved;
                    ViewModel.ConnectionAdded += model_ConnectionAdded;
                    ViewModel.ConnectionRemoved += model_ConnectionRemoved;
                }
            }
        }

        public DiagramEditingContext()
        {
            _selection = new HyperGraph.GraphSelection();
            _selection.SelectionChanging +=_selection_SelectionChanging;
            _selection.SelectionChanged += _selection_SelectionChanged;
        }

        ~DiagramEditingContext()
        {
            ContainingDocument = null;  // (remove callbacks)
        }

        #region ISelectionContext Member
        private void _selection_SelectionChanged(object sender, EventArgs e)
        {
            if (SelectionChanged != null)
                SelectionChanged.Invoke(sender, e);
        }

        private void _selection_SelectionChanging(object sender, EventArgs e)
        {
            if (SelectionChanging != null)
                SelectionChanging.Invoke(sender, e);
        }

        public object LastSelected 
        { 
            get { return _selection.Selection.LastOrDefault(); }
        }
        public IEnumerable<object> Selection 
        { 
            get { return _selection.Selection; }
            set
            {
                _selection.Update(value.AsIEnumerable<HyperGraph.IElement>(), _selection.Selection);
            }
        }
        public int SelectionCount { get { return _selection.Selection.Count; } }

        public event EventHandler SelectionChanged;
        public event EventHandler SelectionChanging;

        public T GetLastSelected<T>() where T : class
        {
            if (typeof(HyperGraph.IElement).IsAssignableFrom(typeof(T)))
                return LastSelected.As<T>();
            return null;
        }
        public IEnumerable<T> GetSelection<T>() where T : class
        {
            return _selection.Selection.AsIEnumerable<T>();
        }
        public bool SelectionContains(object item)
        {
            return _selection.Selection.Contains(item);
        }
        #endregion

        #region IEnumerableContext Members
        IEnumerable<object> IEnumerableContext.Items
        {
            get { return ViewModel.Nodes; }
        }
        #endregion

        #region INamingContext Members
        string INamingContext.GetName(object item)
        {
            var node = item as HyperGraph.Node;
            if (node != null)
                return node.Title;
            return null;
        }

        bool INamingContext.CanSetName(object item)
        {
            return (item as HyperGraph.Node) != null;
        }

        void INamingContext.SetName(object item, string name)
        {
            var node = item as HyperGraph.Node;
            if (node != null)
            {
                node.Title = name;
                return;
            }
        }
        #endregion

        #region IInstancingContext Members
        public virtual bool CanCopy()
        {
            // if (m_instancingContext != null)
            //     return m_instancingContext.CanCopy();
            // return Selection.Count > 0;
            return false;
        }

        public virtual object Copy()
        {
            // We should serialize our selection, and return the serialized text
            // add a serializable format for the system clipboard
            // DomNodeSerializer serializer = new DomNodeSerializer();
            // byte[] data = serializer.Serialize(itemsToCopy.AsIEnumerable<DomNode>());
            // dataObject.SetData(CircuitFormat, data);
            // 
            // return dataObject;
            return null;
        }

        public virtual bool CanInsert(object insertingObject)
        {
            return false;
        }

        public virtual void Insert(object insertingObject)
        {
        }

        public virtual bool CanDelete()
        {
            return false; //  return Selection.Count > 0;
        }

        public virtual void Delete()
        {
        }
        #endregion

        #region IObservableContext Members
        public event EventHandler<ItemInsertedEventArgs<object>> ItemInserted;
        public event EventHandler<ItemRemovedEventArgs<object>> ItemRemoved;
        public event EventHandler<ItemChangedEventArgs<object>> ItemChanged;
        public event EventHandler Reloaded
        {
            add { }
            remove { }
        }

        protected virtual void OnObjectInserted(ItemInsertedEventArgs<object> e)
        {
            ItemInserted.Raise(this, e);
        }
        protected virtual void OnObjectRemoved(ItemRemovedEventArgs<object> e)
        {
            ItemRemoved.Raise(this, e);
        }
        protected virtual void OnObjectChanged(ItemChangedEventArgs<object> e)
        {
            ItemChanged.Raise(this, e);
        }

        private void model_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e)
        {
            OnObjectInserted(new ItemInsertedEventArgs<object>(-1, e.Node, sender));
        }

        private void model_NodeRemoved(object sender, HyperGraph.NodeEventArgs e)
        {
            OnObjectRemoved(new ItemRemovedEventArgs<object>(-1, e.Node, sender));
        }

        private void model_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e)
        {
            OnObjectInserted(new ItemInsertedEventArgs<object>(-1, e.Connection, sender));
        }

        private void model_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e)
        {
            OnObjectRemoved(new ItemRemovedEventArgs<object>(-1, e.Connection, sender));
        }
        #endregion

        #region IColoringContext Members
        Color IColoringContext.GetColor(ColoringTypes kind, object item)
        {
            return s_zeroColor;
        }

        bool IColoringContext.CanSetColor(ColoringTypes kind, object item)
        {
            return false;
        }

        void IColoringContext.SetColor(ColoringTypes kind, object item, Color newValue)
        {
        }
        #endregion

        #region ITreeListView / IItemView Members
        public string[] ColumnNames
        {
            get { return new string[] { "Name" }; }
        }

        public IEnumerable<object> Roots {
            get { return ViewModel.Nodes; }
        }

        public IEnumerable<object> GetChildren(object parent)
        {
            var n = parent as HyperGraph.Node;
            if (n != null)
            {
                foreach (var i in n.InputItems) yield return i;
                foreach (var i in n.CenterItems) yield return i;
                foreach (var i in n.OutputItems) yield return i;
            }
        }

        public void GetInfo(object item, ItemInfo info)
        {
            var n = item as HyperGraph.Node;
            if (n != null)
            {
                info.Label = n.Title;
                info.Properties = new object[0];
                info.HasCheck = false;
                info.FontStyle = FontStyle.Regular;
                info.IsLeaf = false;
                info.IsExpandedInView = false;
                info.HoverText = string.Empty;
                info.AllowLabelEdit = false;
                info.AllowSelect = true;
            }
            else if (NodeFactory != null)
            {
                info.IsLeaf = true;
                info.Label = NodeFactory.GetDescription(item);
            }
            else
            {
                info.IsLeaf = true;
            }
        }
        #endregion

        [Import] public NodeEditorCore.IShaderFragmentNodeCreator NodeFactory;
        private DiagramDocument _containingDocument;

        private static Color s_zeroColor = new Color();
    }
}
