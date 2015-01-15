using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows.Controls;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for the item in a RibbonControl
    /// </summary>
    public class RibbonControlDataAutomationPeer : ItemAutomationPeer
    {
        public RibbonControlDataAutomationPeer(object item, ItemsControlAutomationPeer itemsControlPeer)
            : base(item, itemsControlPeer)
        {
        }
    
        protected override AutomationControlType  GetAutomationControlTypeCore()
        {
            return AutomationControlType.ListItem;
        }

        protected override string  GetClassNameCore()
        {
            AutomationPeer wrapperPeer = GetWrapperPeer();
            if (wrapperPeer != null)
            {
                return wrapperPeer.GetClassName();
            }
            return string.Empty;
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            // In .net4 ItemAutomationPeer implements VirtualizedItemPattern, then we would need to call base.GetPattern here.
            object peer = null;

            // Doesnt implement any patterns of its own, so just forward to the wrapper peer. 
            AutomationPeer wrapperPeer = GetWrapperPeer();
            if (wrapperPeer != null)
            {
                peer = wrapperPeer.GetPattern(patternInterface);
            }

            return peer;
        }

        #region Private methods

        private RibbonControlAutomationPeer GetWrapperPeer()
        {
            RibbonControlAutomationPeer wrapperPeer = null;
            RibbonControl wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapperPeer = UIElementAutomationPeer.CreatePeerForElement(wrapper) as RibbonControlAutomationPeer;
            }
            return wrapperPeer;
        }

        private RibbonControl GetWrapper()
        {
            return this.OwningItemsControl.ItemContainerGenerator.ContainerFromItem(Item) as RibbonControl;
        }

        private ItemsControl OwningItemsControl
        {
            get
            {
                return (ItemsControl)ItemsControlAutomationPeer.Owner;
            }
        }

        #endregion
    }
}
