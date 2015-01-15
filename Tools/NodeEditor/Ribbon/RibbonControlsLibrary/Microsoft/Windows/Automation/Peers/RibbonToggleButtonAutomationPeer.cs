//---------------------------------------------------------------------------
// <copyright file="RibbonToggleButtonAutomationPeer.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Automation.Peers
{
    #region Using declarations

    using System.Windows.Automation.Peers;
    using Microsoft.Windows.Controls.Ribbon;
    using System;

    #endregion

    /// <summary>
    ///   An automation peer class which automates RibbonToggleButton control.
    /// </summary>
    public class RibbonToggleButtonAutomationPeer : ToggleButtonAutomationPeer
    {

        #region Constructors

        /// <summary>
        ///   Defer to the base class for initialization
        /// </summary>
        public RibbonToggleButtonAutomationPeer(RibbonToggleButton owner): base(owner)
        {
        }

        #endregion

        #region AutomationPeer overrides

        /// <summary>
        ///   Get KeyTip of owner control
        /// </summary>
        protected override string GetAccessKeyCore()
        {
            string accessKey = ((RibbonToggleButton)Owner).KeyTip;
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
            return "RibbonToggleButton";
        }


        /// <summary>
        ///   Returns name for automation clients to display
        /// </summary>
        protected override string GetNameCore()
        {
            string name = base.GetNameCore();
            if (String.IsNullOrEmpty(name))
            {
                name = ((RibbonToggleButton)Owner).Label;
            }

            return name;
        }

        /// <summary>
        ///   Returns help text 
        /// </summary>
        protected override string GetHelpTextCore()
        {
            string helpText = base.GetHelpTextCore();
            if (String.IsNullOrEmpty(helpText))
            {
                RibbonToolTip toolTip = ((RibbonToggleButton)Owner).ToolTip as RibbonToolTip;
                if (toolTip != null)
                {
                    helpText = toolTip.Description;
                }
            }

            return helpText;
        }

        #endregion
    }
}
