//*****************************************************************************
//
//  File:       ExecuteEventsArgs.cs
//
//  Contents:   Definition for execute events arguments 
//
//*****************************************************************************

using System;
using RibbonLib.Interop;

namespace RibbonLib.Controls.Events
{
    /// <summary>
    /// Execute event args
    /// </summary>
    public class ExecuteEventArgs : EventArgs
    {
        private PropertyKeyRef _key;
        private PropVariantRef _currentValue;
        private IUISimplePropertySet _commandExecutionProperties;

        public ExecuteEventArgs(PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties)
        {
            _key = key;
            _currentValue = currentValue;
            _commandExecutionProperties = commandExecutionProperties;
        }

        public PropertyKeyRef Key
        {
            get
            {
                return _key;
            }
        }

        public PropVariantRef CurrentValue
        {
            get
            {
                return _currentValue;
            }
        }

        public IUISimplePropertySet CommandExecutionProperties
        {
            get
            {
                return _commandExecutionProperties;
            }                
        }
    }
}
