using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for a RibbonContextualTabGroup
    /// </summary>
    public class RibbonContextualTabGroupAutomationPeer : FrameworkElementAutomationPeer
    {
        public RibbonContextualTabGroupAutomationPeer(RibbonContextualTabGroup owner)
            : base(owner)
        {
        }

        protected override string GetNameCore()
        {
            string name = base.GetNameCore();

            if (String.IsNullOrEmpty(name))
            {
                RibbonContextualTabGroup tabGroup = Owner as RibbonContextualTabGroup;
                if (tabGroup != null && tabGroup.Header != null)
                {
                    UIElement headerElement = tabGroup.Header as UIElement;
                    if (headerElement != null)
                    {
                        AutomationPeer peer = CreatePeerForElement(headerElement);
                        if (peer != null)
                        {
                            name = peer.GetName();
                        }
                    }

                    if (String.IsNullOrEmpty(name))
                    {
                        name = tabGroup.Header.ToString();
                    }
                }
            }

            return name;
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
