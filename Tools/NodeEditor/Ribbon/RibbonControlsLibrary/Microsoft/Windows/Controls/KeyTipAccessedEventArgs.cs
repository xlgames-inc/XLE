//---------------------------------------------------------------------------
// <copyright file="KeyTipAccessedEventArgs.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System;
using System.Windows;
namespace Microsoft.Windows.Controls
{
    /// <summary>
    ///     Event args for KeyTipService.KeyTipAccessedEvent
    /// </summary>
    public class KeyTipAccessedEventArgs : RoutedEventArgs
    {
        public KeyTipAccessedEventArgs()
        {
        }

        /// <summary>
        ///     This property determines what will be the
        ///     next keytip scope after routing this event.
        /// </summary>
        public DependencyObject TargetKeyTipScope { get; set; }

        protected override void InvokeEventHandler(Delegate genericHandler, object genericTarget)
        {
            KeyTipAccessedEventHandler handler = (KeyTipAccessedEventHandler)genericHandler;
            handler(genericTarget, this);
        }
    }

    /// <summary>
    ///     Event handler type for KeyTipService.KeyTipAccessedEvent.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    public delegate void KeyTipAccessedEventHandler(object sender,
                                    KeyTipAccessedEventArgs e);
}