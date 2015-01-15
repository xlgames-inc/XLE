//*****************************************************************************
//
//  File:       ContextAvailablePropertiesProvider.cs
//
//  Contents:   Definition for context available properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for context available properties interface
    /// </summary>
    public interface IContextAvailablePropertiesProvider
    {
        /// <summary>
        /// Context available property
        /// </summary>
        ContextAvailability ContextAvailable { get; set; }
    }

    /// <summary>
    /// Implementation of IContextAvailablePropertiesProvider
    /// </summary>
    public class ContextAvailablePropertiesProvider : BasePropertiesProvider, IContextAvailablePropertiesProvider
    {
        /// <summary>
        /// ContextAvailablePropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public ContextAvailablePropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.ContextAvailable);
        }

        private ContextAvailability? _contextAvailable;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.ContextAvailable)
            {
                if (_contextAvailable.HasValue)
                {
                    newValue.SetUInt((uint)_contextAvailable.Value);
                }
            } 
            
            return HRESULT.S_OK;
        }

        #region IContextAvailablePropertiesProvider Members

        /// <summary>
        /// Context available property
        /// </summary>
        public ContextAvailability ContextAvailable
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant uintValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.ContextAvailable, out uintValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (ContextAvailability)(uint)uintValue.Value;
                    }
                }

                return _contextAvailable.GetValueOrDefault();
            }
            set
            {
                _contextAvailable = value;
                if (_ribbon.Initalized)
                {
                    PropVariant uintValue = PropVariant.FromObject((uint)value);
                    HRESULT hr = _ribbon.Framework.SetUICommandProperty(_commandID, ref RibbonProperties.ContextAvailable, ref uintValue);
                }
            }
        }

        #endregion
    }
}
