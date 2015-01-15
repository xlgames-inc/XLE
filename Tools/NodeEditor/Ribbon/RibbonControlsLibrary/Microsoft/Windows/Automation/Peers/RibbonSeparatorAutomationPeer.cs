using System;
using System.Windows.Automation.Peers;
using Microsoft.Windows.Controls.Ribbon;

namespace Microsoft.Windows.Automation.Peers
{
    /// 
    public class RibbonSeparatorAutomationPeer : SeparatorAutomationPeer
    {
        ///
        public RibbonSeparatorAutomationPeer(RibbonSeparator owner) : base(owner)
        { }

        ///
        protected override string GetClassNameCore()
        {
            return Owner.GetType().Name;
        }

        /// <summary>
        ///   Returns name for automation clients to display
        /// </summary>
        protected override string GetNameCore()
        {
            string name = base.GetNameCore();
            if (String.IsNullOrEmpty(name))
            {
                name = ((RibbonSeparator)Owner).Label;
            }

            return name;
        }

    }
}