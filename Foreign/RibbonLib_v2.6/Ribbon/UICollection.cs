//*****************************************************************************
//
//  File:       UICollection.cs
//
//  Contents:   Helper class that provides an implementation of the 
//              IUIColleciton interface.
//
//*****************************************************************************

using System.Collections.Generic;
using RibbonLib.Interop;

namespace RibbonLib
{
    public class UICollection : IUICollection
    {
        private List<object> _items = new List<object>();

        #region IUICollection Members

        public HRESULT GetCount(out uint count)
        {
            count = (uint)_items.Count;
            return HRESULT.S_OK;
        }

        public HRESULT GetItem(uint index, out object item)
        {
            item = _items[(int)index];
            return HRESULT.S_OK;
        }

        public HRESULT Add(object item)
        {
            _items.Add(item);
            return HRESULT.S_OK;
        }

        public HRESULT Insert(uint index, object item)
        {
            _items.Insert((int)index, item);
            return HRESULT.S_OK;
        }

        public HRESULT RemoveAt(uint index)
        {
            _items.RemoveAt((int)index);
            return HRESULT.S_OK;
        }

        public HRESULT Replace(uint indexReplaced, object itemReplaceWith)
        {
            _items[(int)indexReplaced] = itemReplaceWith;
            return HRESULT.S_OK;
        }

        public HRESULT Clear()
        {
            _items.Clear();
            return HRESULT.S_OK;
        }

        #endregion
    }
}
