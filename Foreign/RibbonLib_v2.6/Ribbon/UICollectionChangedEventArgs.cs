//*****************************************************************************
//
//  File:       UICollectionChangedEventArgs.cs
//
//  Contents:   UI collection changed event arguments
//
//*****************************************************************************

using System;
using RibbonLib.Interop;

namespace RibbonLib
{
    public class UICollectionChangedEventArgs : EventArgs
    {
        private CollectionChange _action;
        private uint _oldIndex;
        private object _oldItem;
        private uint _newIndex;
        private object _newItem;

        public UICollectionChangedEventArgs(CollectionChange action, uint oldIndex, object oldItem, uint newIndex, object newItem)
        {
            _action = action;
            _oldIndex = oldIndex;
            _oldItem = oldItem;
            _newIndex = newIndex;
            _newItem = newItem;
        }

        public CollectionChange Action
        {
            get
            {
                return _action;
            }
        }

        public uint OldIndex
        {
            get
            {
                return _oldIndex;
            }
        }

        public object OldItem
        {
            get
            {
                return _oldItem;
            }
        }

        public uint NewIndex
        {
            get
            {
                return _newIndex;
            }
        }

        public object NewItem
        {
            get
            {
                return _newItem;
            }
        }
    }
}
