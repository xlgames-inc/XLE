//*****************************************************************************
//
//  File:       PreviewEventsProvider.cs
//
//  Contents:   definition for preview events provider 
//
//*****************************************************************************

using RibbonLib.Interop;
using System;

namespace RibbonLib.Controls.Events
{
    /// <summary>
    /// Definition for Execute events provider
    /// </summary>
    public interface IPreviewEventsProvider
    {
        /// <summary>
        /// Preview event
        /// </summary>
        event EventHandler<ExecuteEventArgs> PreviewEvent;

        /// <summary>
        /// Cancel Preview event
        /// </summary>
        event EventHandler<ExecuteEventArgs> CancelPreviewEvent;
    }

    /// <summary>
    /// Implementation of IPreviewEventsProvider
    /// </summary>
    class PreviewEventsProvider : BaseEventsProvider, IPreviewEventsProvider
    {
        private object _sender;

        public PreviewEventsProvider(object sender)
        {
            _sender = sender;
            SupportedEvents.Add(ExecutionVerb.Preview);
            SupportedEvents.Add(ExecutionVerb.CancelPreview);
        }

        /// <summary>
        /// Handles IUICommandHandler.Execute function for supported events
        /// </summary>
        /// <param name="verb">the mode of execution</param>
        /// <param name="key">the property that has changed</param>
        /// <param name="currentValue">the new value of the property that has changed</param>
        /// <param name="commandExecutionProperties">additional data for this execution</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT Execute(ExecutionVerb verb, PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties)
        {
            switch (verb)
            {
                case ExecutionVerb.Preview:
                    if (PreviewEvent != null)
                    {
                        PreviewEvent(_sender, new ExecuteEventArgs(key, currentValue, commandExecutionProperties));
                    }
                    break;

                case ExecutionVerb.CancelPreview:
                    if (CancelPreviewEvent != null)
                    {
                        CancelPreviewEvent(_sender, new ExecuteEventArgs(key, currentValue, commandExecutionProperties));
                    }
                    break;
            }
            return HRESULT.S_OK;
        }

        #region IPreviewEventsProvider Members

        /// <summary>
        /// Preview event
        /// </summary>
        public event EventHandler<ExecuteEventArgs> PreviewEvent;

        /// <summary>
        /// Cancel Preview event
        /// </summary>
        public event EventHandler<ExecuteEventArgs> CancelPreviewEvent;

        #endregion
    }
}
