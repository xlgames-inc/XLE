// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace HyperGraphAdapter
{
    public class DiagramEditingContext :
        IEnumerableContext
        , IInstancingContext
        , IObservableContext
        , INamingContext
        , IColoringContext
        , ISelectionContext
        , ITreeListView
        , IItemView
    {
        public HyperGraph.IGraphModel ViewModel {
            get { return _viewModel; }
            protected set
            {
                if (ViewModel != null)
                {
                    ViewModel.NodeAdded -= model_NodeAdded;
                    ViewModel.NodeRemoved -= model_NodeRemoved;
                    ViewModel.ConnectionAdded -= model_ConnectionAdded;
                    ViewModel.ConnectionRemoved -= model_ConnectionRemoved;
                }

                _viewModel = value;

                if (_viewModel != null)
                {
                    ViewModel.NodeAdded += model_NodeAdded;
                    ViewModel.NodeRemoved += model_NodeRemoved;
                    ViewModel.ConnectionAdded += model_ConnectionAdded;
                    ViewModel.ConnectionRemoved += model_ConnectionRemoved;
                    ViewModel.MiscChange += model_MiscChange;
                }
            }
        }
        public HyperGraph.IGraphSelection DiagramSelection { get; }

        public DiagramEditingContext()
        {
            DiagramSelection = new HyperGraph.GraphSelection();
            DiagramSelection.SelectionChanging += _selection_SelectionChanging;
            DiagramSelection.SelectionChanged += _selection_SelectionChanged;
        }

        ~DiagramEditingContext()
        {
            ViewModel = null;
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
            get { return DiagramSelection.Selection.LastOrDefault(); }
        }
        public IEnumerable<object> Selection
        {
            get { return DiagramSelection.Selection; }
            set
            {
                DiagramSelection.Update(value.AsIEnumerable<HyperGraph.IElement>(), DiagramSelection.Selection);
            }
        }
        public int SelectionCount { get { return DiagramSelection.Selection.Count; } }

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
            return DiagramSelection.Selection.AsIEnumerable<T>();
        }
        public bool SelectionContains(object item)
        {
            return DiagramSelection.Selection.Contains(item);
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

        public IEnumerable<object> Roots
        {
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

        public delegate string GetDescriptionDelegate(object item);
        public GetDescriptionDelegate GetDescription { get; set; }

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
            else if (GetDescription != null)
            {
                info.IsLeaf = true;
                info.Label = GetDescription(item);
            }
            else
            {
                info.IsLeaf = true;
            }
        }
        #endregion

        #region ViewModel Changes
        protected virtual void model_NodeAdded(object sender, HyperGraph.AcceptNodeEventArgs e)
        {
            OnObjectInserted(new ItemInsertedEventArgs<object>(-1, e.Node, sender));
        }

        protected virtual void model_NodeRemoved(object sender, HyperGraph.NodeEventArgs e)
        {
            OnObjectRemoved(new ItemRemovedEventArgs<object>(-1, e.Node, sender));
        }

        protected virtual void model_ConnectionAdded(object sender, HyperGraph.AcceptNodeConnectionEventArgs e)
        {
            OnObjectInserted(new ItemInsertedEventArgs<object>(-1, e.Connection, sender));
        }

        protected virtual void model_ConnectionRemoved(object sender, HyperGraph.NodeConnectionEventArgs e)
        {
            OnObjectRemoved(new ItemRemovedEventArgs<object>(-1, e.Connection, sender));
        }

        protected virtual void model_MiscChange(object sender, HyperGraph.MiscChangeEventArgs e)
        {
        }
        #endregion

        private static Color s_zeroColor = new Color();
        private HyperGraph.IGraphModel _viewModel = null;
    }
}
