using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using System.Collections.Generic;
using Microsoft.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for the item in a RibbonTab
    /// Supports SelectionItem, ExpandCollapse and ScrollItem patterns.
    /// </summary>
    public class RibbonTabDataAutomationPeer : SelectorItemAutomationPeer, ISelectionItemProvider, IExpandCollapseProvider, IScrollItemProvider
    {
        public RibbonTabDataAutomationPeer(object item, RibbonAutomationPeer itemsControlPeer)
            : base(item, itemsControlPeer)
        {
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            object peer = base.GetPattern(patternInterface);
            if (patternInterface == PatternInterface.ExpandCollapse && OwningRibbon.IsMinimized)
            {
                peer = this;
            }
            
            if(patternInterface == PatternInterface.ScrollItem)
            {
                peer = this;
            }

            if (peer == null)
            {
                RibbonTabAutomationPeer wrapperPeer = GetWrapperPeer();
                if (wrapperPeer != null)
                {
                    peer = wrapperPeer.GetPattern(patternInterface);
                }
            }
            return peer;
        }

        #region IExpandCollapseProvider Members
        /// <summary>
        /// If Ribbon.IsMinimized then set Ribbon.IsDropDownOpen to false
        /// </summary>
        void IExpandCollapseProvider.Collapse()
        {
            RibbonTab wrapperTab = GetWrapper();
            if (wrapperTab != null && OwningRibbon.IsMinimized)
            {
                OwningRibbon.IsDropDownOpen = false;
            }
        }

        /// <summary>
        /// If Ribbon.IsMinimized then set Ribbon.IsDropDownOpen to true
        /// </summary>
        void IExpandCollapseProvider.Expand()
        {
            RibbonTab wrapperTab = GetWrapper();
            // Select the tab and display popup
            if (wrapperTab != null && OwningRibbon.IsMinimized)
            {
                wrapperTab.IsSelected = true;
                OwningRibbon.IsDropDownOpen = true;
            }
        }

        /// <summary>
        /// Return Ribbon.IsDropDownOpen
        /// </summary>
        ExpandCollapseState IExpandCollapseProvider.ExpandCollapseState
        {
            get 
            {
                RibbonTab wrapperTab = GetWrapper();
                if (wrapperTab != null && OwningRibbon.IsMinimized)
                {
                    if (wrapperTab.IsSelected && OwningRibbon.IsDropDownOpen)
                    {
                        return ExpandCollapseState.Expanded;
                    }
                    else
                    {
                        return ExpandCollapseState.Collapsed;
                    }
                }

                // When not minimized
                return ExpandCollapseState.Expanded;
            }
        }

        #endregion

        #region ISelectionItemProvider Members

        /// <summary>
        /// RemoveFromSelection not allowed on currently Selected Tab. No op for other Tabs
        /// </summary>
        void ISelectionItemProvider.RemoveFromSelection()
        {
            RibbonTab tab = GetWrapper();
            if (tab != null && tab.IsSelected)
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }
        }

        /// <summary>
        /// AddToSelection not allowed. No op if Tab is already selected. 
        /// </summary>
        void ISelectionItemProvider.AddToSelection()
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            Selector parentSelector = (Selector)(ItemsControlAutomationPeer.Owner);
            if ((parentSelector == null) || (parentSelector.SelectedItem != null && parentSelector.SelectedItem != Item))
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }
        }

        #endregion

        #region IScrollItemProvider Members
        
        void IScrollItemProvider.ScrollIntoView()
        {
            RibbonTab wrapperTab = GetWrapper();
            if (wrapperTab != null && wrapperTab.RibbonTabHeader != null )
            {
                wrapperTab.RibbonTabHeader.BringIntoView();
            }
        }

        #endregion

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.TabItem;
        }

        protected override string GetClassNameCore()
        {
            RibbonTabAutomationPeer wrapperPeer = GetWrapperPeer() as RibbonTabAutomationPeer;
            if (wrapperPeer != null)
            {
                return wrapperPeer.GetClassName();
            }
            return string.Empty;
        }

        #region Private methods
        
        private RibbonTabAutomationPeer GetWrapperPeer()
        {
            RibbonTabAutomationPeer wrapperPeer = null;
            RibbonTab wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapperPeer = UIElementAutomationPeer.CreatePeerForElement(wrapper) as RibbonTabAutomationPeer;
            }
            return wrapperPeer;
        }

        private RibbonTab GetWrapper()
        {
            return this.OwningRibbon.ItemContainerGenerator.ContainerFromItem(Item) as RibbonTab;
        }

        private Ribbon OwningRibbon
        {
            get
            {
                return (Ribbon)ItemsControlAutomationPeer.Owner;
            }
        }
        #endregion
    }
}
