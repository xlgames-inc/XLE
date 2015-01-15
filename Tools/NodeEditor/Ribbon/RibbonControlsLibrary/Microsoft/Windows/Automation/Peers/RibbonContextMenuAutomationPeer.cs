using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows.Controls;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonContextMenu
    /// </summary>
    public class RibbonContextMenuAutomationPeer : ItemsControlAutomationPeer
    {
        public RibbonContextMenuAutomationPeer(RibbonContextMenu owner)
            : base(owner)
        {
        }

        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonMenuItemDataAutomationPeer(item, this);
        }

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Menu;
        }

    }
}
