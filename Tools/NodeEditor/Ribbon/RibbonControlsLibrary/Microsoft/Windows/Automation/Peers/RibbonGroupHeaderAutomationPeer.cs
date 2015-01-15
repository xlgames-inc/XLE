using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer that is a wrapper around RibbonGroup.Header object
    /// </summary>
    public class RibbonGroupHeaderAutomationPeer : FrameworkElementAutomationPeer
    {
        public RibbonGroupHeaderAutomationPeer(FrameworkElement owner)
            : base(owner)
        {
        }

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Header;
        }

        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        protected override string GetNameCore()
        {
            AutomationPeer ribbonGroupPeer = GetParent();
            if (ribbonGroupPeer != null)
            {
                return ribbonGroupPeer.GetName();
            }

            return string.Empty;
        }
    }
}
