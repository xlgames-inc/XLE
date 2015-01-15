using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonTabHeader
    /// </summary>
    public class RibbonTabHeaderAutomationPeer : FrameworkElementAutomationPeer
    {
        public RibbonTabHeaderAutomationPeer(RibbonTabHeader owner)
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

            // Borrowed from fix OffScreen fix in 4.0
            Rect boundingRect = RibbonHelper.CalculateVisibleBoundingRect(Owner);
            return (boundingRect == Rect.Empty || boundingRect.Height == 0 || boundingRect.Width == 0);
        }

    }
}
