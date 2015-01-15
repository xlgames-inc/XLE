//*****************************************************************************
//
//  File:       IEventsProvider.cs
//
//  Contents:   Interface for components that provides events 
//
//*****************************************************************************

using System.Collections.Generic;
using RibbonLib.Interop;

namespace RibbonLib.Controls.Events
{
    public interface IEventsProvider
    {
        /// <summary>
        /// Get supported "execution verbs", or events
        /// </summary>
        IList<ExecutionVerb> SupportedEvents { get; }

        /// <summary>
        /// Handles IUICommandHandler.Execute function for supported events
        /// </summary>
        /// <param name="verb">the mode of execution</param>
        /// <param name="key">the property that has changed</param>
        /// <param name="currentValue">the new value of the property that has changed</param>
        /// <param name="commandExecutionProperties">additional data for this execution</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        HRESULT Execute(ExecutionVerb verb, PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties);
    }
}
