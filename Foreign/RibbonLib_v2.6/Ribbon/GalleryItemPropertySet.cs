//*****************************************************************************
//
//  File:       GalleryItemPropertySet.cs
//
//  Contents:   Helper class that wraps a gallery item simple property set.
//
//*****************************************************************************

using System.Diagnostics;
using RibbonLib.Interop;

namespace RibbonLib
{
    public class GalleryItemPropertySet : IUISimplePropertySet
    {
        private string _label;
        private uint? _categoryID;
        private IUIImage _itemImage;

        public string Label
        {
            get
            {
                return _label;
            }
            set
            {
                _label = value;
            }
        }

        public uint CategoryID
        {
            get
            {
                return _categoryID.GetValueOrDefault(Constants.UI_Collection_InvalidIndex);
            }
            set
            {
                _categoryID = value;
            }
        }

        public IUIImage ItemImage
        {
            get
            {
                return _itemImage;
            }
            set
            {
                _itemImage = value;
            }
        }

        #region IUISimplePropertySet Members

        public HRESULT GetValue(ref PropertyKey key, out PropVariant value)
        {
            if (key == RibbonProperties.Label)
            {
                if ((_label == null) || (_label.Trim() == string.Empty))
                {
                    value = PropVariant.Empty;
                }
                else
                {
                    value = PropVariant.FromObject(_label);
                }
                return HRESULT.S_OK;
            }
            
            if (key == RibbonProperties.CategoryID)
            {
                if (_categoryID.HasValue)
                {
                    value = PropVariant.FromObject(_categoryID.Value);
                }
                else
                {
                    value = PropVariant.Empty;
                }
                return HRESULT.S_OK;
            }

            if (key == RibbonProperties.ItemImage)
            {
                if (_itemImage != null)
                {
                    value = new PropVariant();
                    value.SetIUnknown(_itemImage);
                }
                else
                {
                    value = PropVariant.Empty;
                }
                return HRESULT.S_OK;
            }

            Debug.WriteLine(string.Format("Class {0} does not support property: {1}.", GetType().ToString(), RibbonProperties.GetPropertyKeyName(ref key)));

            value = PropVariant.Empty;
            return HRESULT.E_NOTIMPL;
        }

        #endregion
    }
}
