//---------------------------------------------------------------------------
// <copyright file="RibbonTextBoxAutomationPeer.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Automation.Peers
{
    #region Using declarations

    using System.Windows.Automation;
    using System.Windows.Automation.Peers;
    using System.Windows.Automation.Provider;
    using System.Windows.Threading;
    using Microsoft.Windows.Controls.Ribbon;
    using Microsoft.Windows.Input;
    using System;
    using System.Windows.Input;

    #endregion

    /// <summary>
    ///   An automation peer class which automates RibbonTextBox control.
    /// </summary>
    public  class RibbonTextBoxAutomationPeer : TextBoxAutomationPeer, IInvokeProvider
    {

        #region Constructors

        /// <summary>
        ///   Initialize Automation Peer for RibbonTextBox.
        /// </summary>
        public RibbonTextBoxAutomationPeer(RibbonTextBox owner): base(owner)
        {
        }

        #endregion

        #region AutomationPeer overrides

        /// <summary>
        ///   Get KeyTip of the owner control.
        /// </summary>
        protected override string GetAccessKeyCore()
        {
            string accessKey = ((RibbonTextBox)Owner).KeyTip;
            if (string.IsNullOrEmpty(accessKey))
            {
                accessKey = base.GetAccessKeyCore();
            }
            return accessKey;
        }

        /// <summary>
        ///   Return class name for automation clients to display.
        /// </summary> 
        protected override string GetClassNameCore()
        {
            return "RibbonTextBox";
        }


        /// <summary>
        ///   Returns name for automation clients to display
        /// </summary>
        protected override string GetNameCore()
        {
            string name = base.GetNameCore();
            if (String.IsNullOrEmpty(name))
            {
                name = ((RibbonTextBox)Owner).Label;
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
                RibbonToolTip toolTip = ((RibbonTextBox)Owner).ToolTip as RibbonToolTip;
                if (toolTip != null)
                {
                    helpText = toolTip.Description;
                }
            }

            return helpText;
        }

        /// <summary>
        ///   Returns command's input gesture as the accelerator key
        /// </summary>
        protected override string GetAcceleratorKeyCore()
        {
            string acceleratorKey = base.GetAcceleratorKeyCore();
            if (String.IsNullOrEmpty(acceleratorKey))
            {
                RoutedUICommand uiCommand = ((RibbonTextBox)Owner).Command as RoutedUICommand;
                if (uiCommand != null)
                {
                    acceleratorKey = uiCommand.Text;
                }
            }
            return acceleratorKey;
        }

        /// <summary>
        ///   Return paterns for automation clients to execute.
        /// </summary> 
        override public object GetPattern(PatternInterface patternInterface)
        {
            if (patternInterface == PatternInterface.Invoke)
            {
                return this;
            }
            else
            {
                return base.GetPattern(patternInterface);
            }
        }

        #endregion

        #region IInvokeProvider Members

        void IInvokeProvider.Invoke()
        {
            RibbonTextBox rtb = (RibbonTextBox)Owner;

            if (!rtb.IsEnabled)
            {
                throw new ElementNotEnabledException();
            }

            Dispatcher.BeginInvoke(DispatcherPriority.Input, new DispatcherOperationCallback(delegate(object param)
            {
                RibbonTextBox textBox = (RibbonTextBox)Owner;
                CommandHelpers.InvokeCommandSource(textBox.CommandParameter, null, textBox, CommandOperation.Execute);
                return null;
            }), null);
        }

        #endregion
    }
}
