//---------------------------------------------------------------------------
// <copyright file="RibbonDismissPopupEventArgs.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System;
using System.Windows;
namespace Microsoft.Windows.Controls.Ribbon
{
    /// <summary>
    ///     Event args for DismissPopup event.
    /// </summary>
    public class RibbonDismissPopupEventArgs : RoutedEventArgs
    {
        /// <summary>
        /// This is an instance constructor for the RibbonDismissPopupEventArgs class.  It
        /// is constructed with a reference to the event being raised.
        /// </summary>
        /// <returns>Nothing.</returns>
        public RibbonDismissPopupEventArgs()
            : this(RibbonDismissPopupMode.Always)
        {
        }

        public RibbonDismissPopupEventArgs(RibbonDismissPopupMode dismissMode)
            : base()
        {
            RoutedEvent = RibbonControlService.DismissPopupEvent;
            DismissMode = dismissMode;
        }

        public RibbonDismissPopupMode DismissMode
        {
            get;
            private set;
        }

        /// <summary>
        /// This method is used to perform the proper type casting in order to
        /// call the type-safe RibbonDismissPopupEventArgs delegate for the DismissPopupEvent event.
        /// </summary>
        /// <param name="genericHandler">The handler to invoke.</param>
        /// <param name="genericTarget">The current object along the event's route.</param>
        /// <returns>Nothing.</returns>
        protected override void InvokeEventHandler(Delegate genericHandler, object genericTarget)
        {
            RibbonDismissPopupEventHandler handler = (RibbonDismissPopupEventHandler)genericHandler;
            handler(genericTarget, this);
        }
    }
}