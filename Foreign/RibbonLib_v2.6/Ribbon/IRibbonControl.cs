//*****************************************************************************
//
//  File:       IRibbonControl.cs
//
//  Contents:   Definition of IRibbonControl interface. 
//              Each ribbon control helper class should implement this 
//              interface according to the control's actions and properties.
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib
{
    internal interface IRibbonControl
    {
        /// <summary>
        /// Ribbon control command id
        /// </summary>
        uint CommandID { get; }

        /// <summary>
        /// Handles IUICommandHandler.Execute function for this ribbon control
        /// </summary>
        /// <param name="verb">the mode of execution</param>
        /// <param name="key">the property that has changed</param>
        /// <param name="currentValue">the new value of the property that has changed</param>
        /// <param name="commandExecutionProperties">additional data for this execution</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        HRESULT Execute(ExecutionVerb verb, PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties);

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for this ribbon control
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue);
    }
}
