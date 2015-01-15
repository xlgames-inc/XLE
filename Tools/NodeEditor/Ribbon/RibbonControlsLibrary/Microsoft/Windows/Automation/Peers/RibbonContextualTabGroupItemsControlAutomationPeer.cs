using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonContextualTabGroupItemsControl
    /// </summary>
    public class RibbonContextualTabGroupItemsControlAutomationPeer : ItemsControlAutomationPeer
    {

        public RibbonContextualTabGroupItemsControlAutomationPeer(RibbonContextualTabGroupItemsControl owner)
            : base(owner)
        {
        }

        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonContextualTabGroupDataAutomationPeer(item, this);
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            // Does not allow scrolling
            if (patternInterface == PatternInterface.Scroll)
            {
                return null;
            }
            return base.GetPattern(patternInterface);
        }
    }
}
