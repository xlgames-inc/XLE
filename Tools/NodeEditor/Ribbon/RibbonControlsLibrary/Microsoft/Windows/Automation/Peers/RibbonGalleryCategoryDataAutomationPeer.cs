using System;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;


namespace Microsoft.Windows.Automation.Peers
{
    public class RibbonGalleryCategoryDataAutomationPeer : ItemAutomationPeer,IScrollItemProvider
    {
        #region constructor

        ///
        public RibbonGalleryCategoryDataAutomationPeer(object owner, ItemsControlAutomationPeer itemsControlAutomationPeer)
            : base(owner, itemsControlAutomationPeer)
        {
        }

        #endregion constructor

        #region Automation override

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
        override public object GetPattern(PatternInterface patternInterface)
        {
            if (patternInterface == PatternInterface.ScrollItem)
            {
                return this;
            }

            // Simply use GetWrapperPeer when available while integrating in PresentationFramework.dll
            RibbonGalleryCategory owningCategory = GetOwningRibbonGalleryCategory();
            if (owningCategory != null)
            {
                RibbonGalleryCategoryAutomationPeer owningPeer = (RibbonGalleryCategoryAutomationPeer)UIElementAutomationPeer.CreatePeerForElement(owningCategory);
                if (owningPeer != null)
                    return owningPeer.GetPattern(patternInterface);
            }
            
            return null;
        }

        #endregion Automation override

        #region internal helper

        // Provides the visual UIElement which contains the CategoryData
        internal RibbonGalleryCategory GetOwningRibbonGalleryCategory()
        {
            RibbonGalleryCategory owningRibbonGalleryCategory = null;
            ItemsControlAutomationPeer itemsControlAutomationPeer = ItemsControlAutomationPeer;
            if (itemsControlAutomationPeer != null)
            {
                RibbonGallery gallery = (RibbonGallery)(itemsControlAutomationPeer.Owner);
                if (gallery != null)
                {
                    owningRibbonGalleryCategory = (RibbonGalleryCategory)gallery.ItemContainerGenerator.ContainerFromItem(Item);
                }
            }
            return owningRibbonGalleryCategory;
        }

        #endregion internal helper

        #region IScrollItemProvider Members

        /// <summary>
        /// call wrapper.BringIntoView
        /// </summary>
        void IScrollItemProvider.ScrollIntoView()
        {
            RibbonGalleryCategory category = GetOwningRibbonGalleryCategory();
            if (category != null)
            {
                category.BringIntoView();
            }
        }

        #endregion

    }
}
