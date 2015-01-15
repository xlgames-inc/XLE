//*****************************************************************************
//
//  File:       BooleanValuePropertyProvider.cs
//
//  Contents:   Definition for boolean value properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for boolean value properties provider interface
    /// </summary>
    public interface IBooleanValuePropertyProvider
    {
        /// <summary>
        /// Boolean value property
        /// </summary>
        bool BooleanValue { get; set; }
    }

    /// <summary>
    /// Implementation of IBooleanValuePropertyProvider
    /// </summary>
    public class BooleanValuePropertyProvider : BasePropertiesProvider, IBooleanValuePropertyProvider
    {
        /// <summary>
        /// BooleanValuePropertyProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public BooleanValuePropertyProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.BooleanValue);
        }

        private bool? _booleanValue;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.BooleanValue)
            {
                if (_booleanValue.HasValue)
                {
                    newValue.SetBool(_booleanValue.Value);
                }
            }
            
            return HRESULT.S_OK;
        }

        #region IBooleanValuePropertyProvider Members

        /// <summary>
        /// Boolean value property
        /// </summary>
        public bool BooleanValue
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant boolValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.BooleanValue, out boolValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (bool)boolValue.Value;
                    }
                }
                return _booleanValue.GetValueOrDefault();
            }
            set
            {
                _booleanValue = value;
                if (_ribbon.Initalized)
                {
                    PropVariant boolValue = PropVariant.FromObject(value);
                    HRESULT hr = _ribbon.Framework.SetUICommandProperty(_commandID, ref RibbonProperties.BooleanValue, ref boolValue);
                }
            }
        }

        #endregion
    }
}
