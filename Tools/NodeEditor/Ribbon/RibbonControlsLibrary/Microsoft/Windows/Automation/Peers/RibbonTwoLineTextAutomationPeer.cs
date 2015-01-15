using System;
using System.Windows.Automation.Peers;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// 
    public class RibbonTwoLineTextAutomationPeer : FrameworkElementAutomationPeer
    {
        ///
        public RibbonTwoLineTextAutomationPeer(RibbonTwoLineText owner) : base(owner)
        { }

        ///
        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        ///
        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Text;
        }

        protected override bool IsControlElementCore()
        {
            // Return true if TextBlock is not part of the style
            return ((RibbonTwoLineText)Owner).TemplatedParent == null;
        }

        /// <summary>
        ///   Returns name for automation clients to display
        /// </summary>
        protected override string GetNameCore()
        {
            string name = base.GetNameCore();
            if (String.IsNullOrEmpty(name))
            {
                name = ((RibbonTwoLineText)Owner).Text;
            }

            return name;
        }

    }
}