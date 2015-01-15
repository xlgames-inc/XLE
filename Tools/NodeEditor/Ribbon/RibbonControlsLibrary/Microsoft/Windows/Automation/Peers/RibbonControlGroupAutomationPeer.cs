using System;
using System.Windows.Automation.Peers;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonControlGroup
    /// </summary>
    public class RibbonControlGroupAutomationPeer : ItemsControlAutomationPeer
    {
        public RibbonControlGroupAutomationPeer(RibbonControlGroup owner)
            : base(owner)
        {
        }

        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonControlDataAutomationPeer(item, this);
        }

        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Group;
        }
    }
}
