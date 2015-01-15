using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Automation;
using System.Collections.Generic;
using Microsoft.Windows.Controls;

namespace Microsoft.Windows.Automation.Peers
{
    /// <summary>
    /// AutomationPeer for the item in a RibbonTabHeader
    /// </summary>
    public class RibbonTabHeaderDataAutomationPeer : ItemAutomationPeer
    {
        public RibbonTabHeaderDataAutomationPeer(object item, RibbonTabHeaderItemsControlAutomationPeer itemsControlPeer)
            : base(item, itemsControlPeer)
        {
        }
        
        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Header;
        }

        protected override string GetClassNameCore()
        {
            RibbonTabHeaderAutomationPeer wrapperPeer = GetWrapperPeer();
            if (wrapperPeer != null)
            {
                return wrapperPeer.GetClassName();
            }
            return string.Empty;
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            return null;
        }

        #region Private methods

        private RibbonTabHeaderAutomationPeer GetWrapperPeer()
        {
            RibbonTabHeaderAutomationPeer wrapperPeer = null;
            RibbonTabHeader wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapperPeer = UIElementAutomationPeer.CreatePeerForElement(wrapper) as RibbonTabHeaderAutomationPeer;
            }
            return wrapperPeer;
        }

        private RibbonTabHeader GetWrapper()
        {
            return ((ItemsControl)ItemsControlAutomationPeer.Owner).ItemContainerGenerator.ContainerFromItem(Item) as RibbonTabHeader;
        }

        #endregion
    }
}
