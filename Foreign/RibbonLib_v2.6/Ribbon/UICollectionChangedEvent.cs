//*****************************************************************************
//
//  File:       UICollectionChangedEvent.cs
//
//  Contents:   Helper class that exposes an OnChanged event for a given 
//              IUICollector instance.
//
//*****************************************************************************

using System;
using System.Runtime.InteropServices.ComTypes;
using RibbonLib.Interop;

namespace RibbonLib
{
   
    public class UICollectionChangedEvent : IUICollectionChangedEvent
    {
        private IUICollection _collection;

        private int _cookie;

        /// <summary>
        /// Attach to an IUICollection object events
        /// </summary>
        /// <param name="collection">IUICollection object</param>
        public void Attach(IUICollection collection)
        {
            if (_collection != null)
            {
                Detach();
            }

            _collection = collection;

            _cookie = RegisterComEvent(_collection);
        }

        /// <summary>
        /// Detach from the previous IUICollection object events
        /// </summary>
        public void Detach()
        {
            if (_collection != null)
            {
                UnregisterComEvent(_collection, _cookie);
                _collection = null;
                _cookie = 0;
            }
        }
        
        public event EventHandler<UICollectionChangedEventArgs> ChangedEvent;

        private IConnectionPoint GetConnectionPoint(IUICollection collection)
        {
            // get connection point container
            IConnectionPointContainer connectionPointContainer = (IConnectionPointContainer)collection;

            // get connection point for IUICollectionChangedEvent
            Guid guid = new Guid(RibbonIIDGuid.IUICollectionChangedEvent);
            IConnectionPoint connectionPoint;
            connectionPointContainer.FindConnectionPoint(ref guid, out connectionPoint);

            return connectionPoint;
        }

        private int RegisterComEvent(IUICollection collection)
        {
            IConnectionPoint connectionPoint = GetConnectionPoint(collection);

            int cookie;
            connectionPoint.Advise(this, out cookie);

            return cookie;
        }

        private void UnregisterComEvent(IUICollection collection, int cookie)
        {
            IConnectionPoint connectionPoint = GetConnectionPoint(collection);

            connectionPoint.Unadvise(cookie);
        }

        #region IUICollectionChangedEvent Members

        HRESULT IUICollectionChangedEvent.OnChanged(CollectionChange action, uint oldIndex, object oldItem, uint newIndex, object newItem)
        {
            if (ChangedEvent != null)
            {
                ChangedEvent(_collection, new UICollectionChangedEventArgs(action, oldIndex, oldItem, newIndex, newItem));
            }
            return HRESULT.S_OK;
        }

        #endregion
    }
}
