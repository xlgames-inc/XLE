//*****************************************************************************
//
//  File:       BasePropertiesProvider.cs
//
//  Contents:   Base class for all properties provider classes.
//              provides common members like: _ribbon, _command_id 
//              and _supportedProperties.
//
//*****************************************************************************

using System.Collections.Generic;
using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    public class BasePropertiesProvider : IPropertiesProvider
    {
        /// <summary>
        /// reference for parent ribbon class
        /// </summary>
        protected Ribbon _ribbon;

        /// <summary>
        /// ribbon control command id
        /// </summary>
        protected uint _commandID;

        /// <summary>
        /// list of supported properties
        /// </summary>
        protected List<PropertyKey> _supportedProperties = new List<PropertyKey>();

        /// <summary>
        /// BasePropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandID">command id attached to this control</param>
        protected BasePropertiesProvider(Ribbon ribbon, uint commandID)
        {
            _ribbon = ribbon;
            _commandID = commandID;
        }

        #region IPropertiesProvider Members

        /// <summary>
        /// Get supported properties
        /// </summary>
        public IList<PropertyKey> SupportedProperties
        {
            get 
            {
                return _supportedProperties;
            }
        }

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public virtual HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            return HRESULT.S_OK;
        }

        #endregion
    }
}
