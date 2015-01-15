//---------------------------------------------------------------------------
// <copyright file="RibbonMenuItemAutomationPeer.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Automation.Peers
{
    #region Using declarations

    using System.Windows.Automation.Peers;
    using Microsoft.Windows.Controls.Ribbon;
    using System;
    using System.Windows.Automation;

    #endregion

    /// <summary>
    ///   An automation peer class which automates RibbonMenuItem control.
    /// </summary>
    public class RibbonMenuItemAutomationPeer : ItemsControlAutomationPeer
    {

        #region Constructors

        /// <summary>
        ///   Initialize Automation Peer for RibbonMenuItem
        /// </summary>
        public RibbonMenuItemAutomationPeer(RibbonMenuItem owner)
            : base(owner)
        {
        }

        #endregion

        #region AutomationPeer overrides

        /// <summary>
        ///   Get KeyTip of the owner control.
        /// </summary>
        protected override string GetAccessKeyCore()
        {
            string accessKey = ((RibbonMenuItem)Owner).KeyTip;
            if (string.IsNullOrEmpty(accessKey))
            {
                accessKey = base.GetAccessKeyCore();
            }
            return accessKey;
        }

        /// <summary>
        ///   Return class name for automation clients to display
        /// </summary> 
        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        /// <summary>
        ///   Returns help text 
        /// </summary>
        protected override string GetHelpTextCore()
        {
            string helpText = base.GetHelpTextCore();
            if (String.IsNullOrEmpty(helpText))
            {
                RibbonToolTip toolTip = ((RibbonMenuItem)Owner).ToolTip as RibbonToolTip;
                if (toolTip != null)
                {
                    helpText = toolTip.Description;
                }
            }

            return helpText;
        }

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.MenuItem;
        }

        #endregion

        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonMenuItemDataAutomationPeer(item, this);
        }

        // Never inline, as we don't want to unnecessarily link the 
        // automation DLL via the ISelectionProvider interface type initialization.
        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
        internal void RaiseExpandCollapseAutomationEvent(bool oldValue, bool newValue)
        {
            AutomationPeer dataPeer = EventsSource;

            if (dataPeer != null)
            {
                dataPeer.RaisePropertyChangedEvent(
                    ExpandCollapsePatternIdentifiers.ExpandCollapseStateProperty,
                    oldValue ? ExpandCollapseState.Expanded : ExpandCollapseState.Collapsed,
                    newValue ? ExpandCollapseState.Expanded : ExpandCollapseState.Collapsed);
            }
        }

        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.NoInlining)]
        internal void RaiseToggleStatePropertyChangedEvent(bool oldValue, bool newValue)
        {
            AutomationPeer dataPeer = EventsSource;

            if (dataPeer != null)
            {
                dataPeer.RaisePropertyChangedEvent(TogglePatternIdentifiers.ToggleStateProperty, ConvertToToggleState(oldValue), ConvertToToggleState(newValue));
            }
        }

        private static ToggleState ConvertToToggleState(bool value)
        {
            switch (value)
            {
                case (true): return ToggleState.On;
                case (false): return ToggleState.Off;
                default: return ToggleState.Indeterminate;
            }
        }
    }
}
