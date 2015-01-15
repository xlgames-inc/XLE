using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Automation.Peers;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    public class RibbonToolTipAutomationPeer : ToolTipAutomationPeer
    {
        #region Constructors

        /// <summary>
        ///   Initialize Automation Peer for RibbonToolTip
        /// </summary>
        public RibbonToolTipAutomationPeer(RibbonToolTip owner): base(owner)
        {
        }

        #endregion

        #region AutomationPeer overrides

        /// <summary>
        ///   Return class name for automation clients to display
        /// </summary> 
        protected override string GetClassNameCore()
        {
            return "RibbonToolTip";
        }


        /// <summary>
        ///   Returns name for automation clients to display
        /// </summary>
        protected override string GetNameCore()
        {
            string name = base.GetNameCore();
            if (String.IsNullOrEmpty(name))
            {
                name = ((RibbonToolTip)Owner).Title;
            }

            return name;
        }

        #endregion
    }
}
