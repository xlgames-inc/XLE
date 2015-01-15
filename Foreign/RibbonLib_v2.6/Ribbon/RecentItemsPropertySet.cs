//*****************************************************************************
//
//  File:       RecentItemsPropertySet.cs
//
//  Contents:   Helper class that wraps a recent items simple property set.
//
//*****************************************************************************

using System.Diagnostics;
using RibbonLib.Interop;

namespace RibbonLib
{
    public class RecentItemsPropertySet : IUISimplePropertySet
    {
        private string _label;
        private string _labelDescription;
        private bool? _pinned;

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

        public string LabelDescription
        {
            get
            {
                return _labelDescription;
            }
            set
            {
                _labelDescription = value;
            }
        }

        public bool Pinned
        {
            get
            {
                return _pinned.GetValueOrDefault();
            }
            set
            {
                _pinned = value;
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

            if (key == RibbonProperties.LabelDescription)
            {
                if ((_labelDescription == null) || (_labelDescription.Trim() == string.Empty))
                {
                    value = PropVariant.Empty;
                }
                else
                {
                    value = PropVariant.FromObject(_labelDescription);
                }
                return HRESULT.S_OK;
            }
            
            if (key == RibbonProperties.Pinned)
            {
                if (_pinned.HasValue)
                {
                    value = PropVariant.FromObject(_pinned.Value);
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
