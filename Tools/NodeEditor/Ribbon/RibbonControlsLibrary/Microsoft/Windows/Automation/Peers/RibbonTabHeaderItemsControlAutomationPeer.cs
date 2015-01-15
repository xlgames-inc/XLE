using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;
using System.Collections.Generic;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonTabHeaderItemsControl
    /// </summary>
    public class RibbonTabHeaderItemsControlAutomationPeer : ItemsControlAutomationPeer
    {
        public RibbonTabHeaderItemsControlAutomationPeer(RibbonTabHeaderItemsControl owner)
            : base(owner)
        {
        }

        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonTabHeaderDataAutomationPeer(item, this);
        }
    }
}
