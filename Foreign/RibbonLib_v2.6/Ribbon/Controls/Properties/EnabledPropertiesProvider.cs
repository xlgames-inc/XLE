//*****************************************************************************
//
//  File:       EnabledPropertiesProvider.cs
//
//  Contents:   Definition for enabled properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for enabled properties provider interface
    /// </summary>
    public interface IEnabledPropertiesProvider
    {
        /// <summary>
        /// Enabled property
        /// </summary>
        bool Enabled { get; set; }
    }

    /// <summary>
    /// Implementation of IEnabledPropertiesProvider
    /// </summary>
    public class EnabledPropertiesProvider : BasePropertiesProvider, IEnabledPropertiesProvider
    {
        /// <summary>
        /// EnabledPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public EnabledPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.Enabled);
        }

        private bool? _enabled;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.Enabled)
            {
                if (_enabled.HasValue)
                {
                    newValue.SetBool(_enabled.Value);
                }
            } 
            
            return HRESULT.S_OK;
        }

        #region IEnabledPropertiesProvider Members

        /// <summary>
        /// Enabled property
        /// </summary>
        public bool Enabled
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant boolValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.Enabled, out boolValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (bool)boolValue.Value;
                    }
                }

                return _enabled.GetValueOrDefault();
            }
            set
            {
                _enabled = value;
                if (_ribbon.Initalized)
                {
                    PropVariant boolValue = PropVariant.FromObject(value);
                    HRESULT hr = _ribbon.Framework.SetUICommandProperty(_commandID, ref RibbonProperties.Enabled, ref boolValue);
                }
            }
        }

        #endregion
    }
}
