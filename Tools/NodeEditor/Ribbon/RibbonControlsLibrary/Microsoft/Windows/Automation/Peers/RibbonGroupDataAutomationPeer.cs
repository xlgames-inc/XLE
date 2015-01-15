using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows.Automation;
using System.Collections.Generic;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for the item in a RibbonGroup.
    /// Supports ScrollItem and ExpandCollapse Patterns.
    /// </summary>
    public class RibbonGroupDataAutomationPeer : ItemAutomationPeer, IScrollItemProvider, IExpandCollapseProvider
    {
        public RibbonGroupDataAutomationPeer(object item, RibbonTabAutomationPeer itemsControlPeer)
            : base(item, itemsControlPeer)
        {
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            // In .net4 ItemAutomationPeer implements VirtualizedItemPattern, then we would need to call base.GetPattern here.
            object peer = null;

            if (patternInterface == PatternInterface.ScrollItem)
            {
                peer = this;
            }
            else if (patternInterface == PatternInterface.ExpandCollapse)
            {
                // only if RibbonGroup is Collapsed this Pattern applies.
                RibbonGroup wrapperGroup = GetWrapper();
                if (wrapperGroup != null && wrapperGroup.IsCollapsed)
                {
                    peer = this;
                }
            }

            if (peer == null)
            {
                RibbonGroupAutomationPeer wrapperPeer = GetWrapperPeer();
                if (wrapperPeer != null)
                {
                    peer = wrapperPeer.GetPattern(patternInterface);
                }
            }
            return peer;
        }
        

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Group;
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

        #region IScrollItemProvider Members

        void IScrollItemProvider.ScrollIntoView()
        {
            RibbonGroup wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapper.BringIntoView();
            }
        }

        #endregion


        #region IExpandCollapseProvider Members

        /// <summary>
        /// Close Popup
        /// </summary>
        void IExpandCollapseProvider.Collapse()
        {
            RibbonGroup wrapperGroup = GetWrapper();
            if (wrapperGroup != null && wrapperGroup.IsCollapsed)
            {
                wrapperGroup.IsDropDownOpen = false;
            }
        }

        /// <summary>
        /// Open popup
        /// </summary>
        void IExpandCollapseProvider.Expand()
        {
            RibbonGroup wrapperGroup = GetWrapper();
            if (wrapperGroup != null && wrapperGroup.IsCollapsed)
            {
                wrapperGroup.IsDropDownOpen = true;
            }
        }

        /// <summary>
        /// Return IsDropDownOpen
        /// </summary>
        ExpandCollapseState IExpandCollapseProvider.ExpandCollapseState
        {
            get
            {
                RibbonGroup wrapperGroup = GetWrapper();
                if (wrapperGroup != null && wrapperGroup.IsCollapsed)
                {
                    return wrapperGroup.IsDropDownOpen ? ExpandCollapseState.Expanded : ExpandCollapseState.Collapsed;
                }

                return ExpandCollapseState.LeafNode;
            }
        }

        #endregion 

        #region Private methods
        
        internal RibbonGroupAutomationPeer GetWrapperPeer()
        {
            RibbonGroup wrapper = GetWrapper();
            if (wrapper != null)
            {
                return UIElementAutomationPeer.CreatePeerForElement(wrapper) as RibbonGroupAutomationPeer;
            }
            return null;
        }

       
        private RibbonGroup GetWrapper()
        {
            return this.OwningTab.ItemContainerGenerator.ContainerFromItem(Item) as RibbonGroup;
        }

        private RibbonTab OwningTab
        {
            get { return (RibbonTab)ItemsControlAutomationPeer.Owner; }
        }
        #endregion

        
    }
}

