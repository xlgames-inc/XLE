using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for RibbonControl
    /// </summary>
    public class RibbonControlAutomationPeer : FrameworkElementAutomationPeer
    {
        public RibbonControlAutomationPeer(FrameworkElement owner)
            : base(owner)
        {
        }

        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        ///
        override protected bool IsOffscreenCore()
        {
            if (!Owner.IsVisible)
                return true;

            Rect boundingRect = RibbonHelper.CalculateVisibleBoundingRect(Owner);
            return (boundingRect == Rect.Empty || boundingRect.Height == 0 || boundingRect.Width == 0);
        }
    }
}
