using System;
using System.Windows;
using System.Windows.Controls;
using System.Collections.Generic;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;



namespace Microsoft.Windows.Automation.Peers
{
    public class RibbonGalleryCategoryAutomationPeer : ItemsControlAutomationPeer
    {
        #region constructor
        ///
        public RibbonGalleryCategoryAutomationPeer(RibbonGalleryCategory owner)
            : base(owner)
        {}

        #endregion constructor

        #region AutomationPeer overrides

        ///
        protected override List<AutomationPeer> GetChildrenCore()
        {
            ItemsControl owner = (ItemsControl)Owner;

            if (!owner.IsGrouping)
            {
                return base.GetChildrenCore();
            }

            return null;
        }

        ///
        public override object GetPattern(PatternInterface patternInterface)
        {
            return base.GetPattern(patternInterface);
        }

        ///
        override protected string GetClassNameCore()
        {
            return "RibbonGalleryCategory";
        }

        ///
        override protected AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.Group;
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

        #endregion AutomationPeer overrides

        #region ItemsControl overrides
        ///
        protected override ItemAutomationPeer CreateItemAutomationPeer(object item)
        {
            return new RibbonGalleryItemDataAutomationPeer(item, this, EventsSource as RibbonGalleryCategoryDataAutomationPeer);
        }

        #endregion ItemsControl overrides
    }
}
