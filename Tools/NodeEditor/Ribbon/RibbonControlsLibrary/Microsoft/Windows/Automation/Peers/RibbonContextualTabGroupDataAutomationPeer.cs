using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for the item in a RibbonContextualTabGroup.
    /// Supports Invoke Pattern.
    /// </summary>
    public class RibbonContextualTabGroupDataAutomationPeer : ItemAutomationPeer, IInvokeProvider
    {
        public RibbonContextualTabGroupDataAutomationPeer(object item, RibbonContextualTabGroupItemsControlAutomationPeer owner)
            : base(item, owner)
        {
        }

        #region IInvokeProvider Members
        /// <summary>
        /// Selects the first tab in the ContextualTabGroup
        /// </summary>
        void IInvokeProvider.Invoke()
        {
            RibbonContextualTabGroup group = GetWrapper();
            // Select the first Tab
            if (group != null && group.Ribbon != null)
            {
                group.Ribbon.NotifyMouseClickedOnContextualTabGroup(group);
            }
        }

        #endregion

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Header;
        }

        protected override string GetClassNameCore()
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
            if (patternInterface == PatternInterface.Invoke)
                return this;
            return null;
        }

        #region Private methods
        private RibbonContextualTabGroupAutomationPeer GetWrapperPeer()
        {
            RibbonContextualTabGroupAutomationPeer wrapperPeer = null;
            RibbonContextualTabGroup wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapperPeer = UIElementAutomationPeer.CreatePeerForElement(wrapper) as RibbonContextualTabGroupAutomationPeer;
            }
            return wrapperPeer;
        }

        private RibbonContextualTabGroup GetWrapper()
        {
            ItemsControl parentItemsControl = (ItemsControl)ItemsControlAutomationPeer.Owner;
            return parentItemsControl.ItemContainerGenerator.ContainerFromItem(Item) as RibbonContextualTabGroup;
        }

        #endregion
    }
}
