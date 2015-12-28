// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

namespace MaterialTool
{
    /// <summary>
    /// Class that defines a circuit editing context. Each context represents a circuit,
    /// with a history, selection, and editing capabilities. There may be multiple
    /// contexts within a single circuit document, because each sub-circuit has its own
    /// editing context.</summary>
    public class ShaderEditingContext :
        EditingContext      // (provides some implementation for managing selection and history)
        , IEnumerableContext
        , IInstancingContext
        , IObservableContext
        , INamingContext
        , IColoringContext
    {
        #region IEnumerableContext Members

        /// <summary>
        /// Gets an enumeration of all of the items of this context</summary>
        IEnumerable<object> IEnumerableContext.Items
        {
            get
            {
                foreach (Element module in CircuitContainer.Elements)
                    yield return module;
                foreach (Wire connection in CircuitContainer.Wires)
                    yield return connection;
                if (CircuitContainer.Annotations != null)
                {
                    foreach (Annotation annotation in CircuitContainer.Annotations)
                        yield return annotation;
                }
            }
        }

        #endregion

        #region INamingContext Members

        /// <summary>
        /// Gets the item's name in the context, or null if none</summary>
        /// <param name="item">Item</param>
        /// <returns>Item's name in the context, or null if none</returns>
        string INamingContext.GetName(object item)
        {
            Element element = item.As<Element>();
            if (element != null)
                return element.Name;

            Wire wire = item.As<Wire>();
            if (wire != null)
                return wire.Label;

            var groupPin = item.As<GroupPin>();
            if (groupPin != null)
                return groupPin.Name;
            return null;
        }

        /// <summary>
        /// Returns whether the item can be named</summary>
        /// <param name="item">Item to name</param>
        /// <returns>True iff the item can be named</returns>
        bool INamingContext.CanSetName(object item)
        {
            return
                item.Is<Element>() ||
                item.Is<Wire>() ||
                item.Is<GroupPin>();
        }

        /// <summary>
        /// Sets the item's name</summary>
        /// <param name="item">Item to name</param>
        /// <param name="name">New item name</param>
        void INamingContext.SetName(object item, string name)
        {
            Element element = item.As<Element>();
            if (element != null)
            {
                element.Name = name;
                return;
            }

            Wire wire = item.As<Wire>();
            if (wire != null)
            {
                wire.Label = name;
                return;
            }


            var groupPin = item.As<GroupPin>();
            if (groupPin != null)
            {
                groupPin.Name = name;
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
            return Selection.Count > 0;
        }

        public virtual void Delete()
        {
        }

        #endregion

        #region IObservableContext Members

        /// <summary>
        /// Event that is raised when an item is inserted</summary>
        public event EventHandler<ItemInsertedEventArgs<object>> ItemInserted;

        /// <summary>
        /// Event that is raised when an item is removed</summary>
        public event EventHandler<ItemRemovedEventArgs<object>> ItemRemoved;

        /// <summary>
        /// Event that is raised when an item is changed</summary>
        public event EventHandler<ItemChangedEventArgs<object>> ItemChanged;

        /// <summary>
        /// Event that is raised when collection has been reloaded</summary>
        public event EventHandler Reloaded
        {
            add { }
            remove { }
        }

        /// <summary>
        /// Raises the ItemInserted event and performs custom processing</summary>
        /// <param name="e">ItemInsertedEventArgs containing event data</param>
        protected virtual void OnObjectInserted(ItemInsertedEventArgs<object> e)
        {
            ItemInserted.Raise(this, e);
        }

        /// <summary>
        /// Raises the ItemRemoved event and performs custom processing</summary>
        /// <param name="e">ItemRemovedEventArgs containing event data</param>
        protected virtual void OnObjectRemoved(ItemRemovedEventArgs<object> e)
        {
            ItemRemoved.Raise(this, e);
        }

        /// <summary>
        /// Raises the ItemChanged event and performs custom processing</summary>
        /// <param name="e">ItemChangedEventArgs containing event data</param>
        protected virtual void OnObjectChanged(ItemChangedEventArgs<object> e)
        {
            ItemChanged.Raise(this, e);
        }

        /// <summary>
        /// Explicitly notify the graph object has been changed. This is useful when some changes, 
        /// such as group pin connectivity, are computed at runtime, outside DOM attribute mechanism.</summary>
        /// <param name="element">Changed graph object</param>
        public void NotifyObjectChanged(object element)
        {
            OnObjectChanged(new ItemChangedEventArgs<object>(element));
        }

        private static bool IsCircuitItem(DomNode child, DomNode parent)
        {
            if (parent == null)
                return false;

            while (parent != null &&
                parent.Is<LayerFolder>())
            {
                parent = parent.Parent;
            }

            return
               child.Is<Group>() || parent.Is<Circuit>() || parent.Is<Group>();
        }


        private void GroupChanged(object sender, EventArgs eventArgs)
        {

            var group = sender.Cast<Group>();
            // Some group properties, such as its display bound, are computed at runtime but not stored as Dom Attributes.  
            // Since group elements could be nested and child changes could affect the appearance of the parent group,  
            // need to bubble up the ItemChanged event by traversing up the editing context chain 
            foreach (var node in group.DomNode.Lineage)
            {
                var editingContext = node.As<CircuitEditingContext>();
                if (editingContext != null)
                    editingContext.ItemChanged.Raise(this, new ItemChangedEventArgs<object>(group.DomNode));
            }

        }

        #endregion

        #region IColoringContext Members

        /// <summary>
        /// Gets the item's specified color in the context</summary>
        /// <param name="kind">Coloring type</param>
        /// <param name="item">Item</param>
        Color IColoringContext.GetColor(ColoringTypes kind, object item)
        {
            if (item.Is<Annotation>())
            {
                if (kind == ColoringTypes.BackColor)
                    return item.Cast<Annotation>().BackColor;
                if (kind == ColoringTypes.ForeColor)
                    return item.Cast<Annotation>().ForeColor;
            }

            return s_zeroColor;
        }

        /// <summary>
        /// Returns whether the item can be colored</summary>
        /// <param name="kind">Coloring type</param>
        /// <param name="item">Item to color</param>
        /// <returns>True iff the item can be colored</returns>
        bool IColoringContext.CanSetColor(ColoringTypes kind, object item)
        {
            if (item.Is<Annotation>())
            {
                if (kind == ColoringTypes.BackColor)
                    return true;
                if (kind == ColoringTypes.ForeColor)
                    return true;
            }

            return false;
        }

        /// <summary>
        /// Sets the item's color</summary>
        /// <param name="kind">Coloring type</param>
        /// <param name="item">Item to name</param>
        /// <param name="newValue">Item new color</param>
        void IColoringContext.SetColor(ColoringTypes kind, object item, Color newValue)
        {
            if (item.Is<Annotation>())
            {
                if (kind == ColoringTypes.BackColor)
                    item.Cast<Annotation>().BackColor = newValue;
                else if (kind == ColoringTypes.ForeColor)
                    item.Cast<Annotation>().ForeColor = newValue;
            }
        }

        #endregion

        private static Color s_zeroColor = new Color();
    }
}
