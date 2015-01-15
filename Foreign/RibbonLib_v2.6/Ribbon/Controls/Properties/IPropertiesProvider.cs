//*****************************************************************************
//
//  File:       IPropertiesProvider.cs
//
//  Contents:   Interface for components that provides properties 
//
//*****************************************************************************

using System.Collections.Generic;
using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    public interface IPropertiesProvider
    {
        /// <summary>
        /// Get supported properties
        /// </summary>
        IList<PropertyKey> SupportedProperties { get; }
        
        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue);
    }
}
