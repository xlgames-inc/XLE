using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows;
using System.Windows.Controls;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer that is a wrapper around Ribbon.Title object
    /// </summary>
    public class RibbonTitleAutomationPeer : FrameworkElementAutomationPeer
    {
        public RibbonTitleAutomationPeer(FrameworkElement owner)
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
            ContentPresenter cp = Owner as ContentPresenter;
            if (cp != null && cp.Content != null)
            {
                return cp.Content.ToString();
            }
            return base.GetNameCore();
        }
    }
}
