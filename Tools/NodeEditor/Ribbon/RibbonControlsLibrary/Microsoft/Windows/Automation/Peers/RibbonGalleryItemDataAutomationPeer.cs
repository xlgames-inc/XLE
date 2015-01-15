using System;
using System.Windows.Automation;
using System.Windows.Automation.Peers;
using System.Windows.Automation.Provider;
using Microsoft.Windows.Controls.Ribbon;
using System.Windows.Controls;


namespace Microsoft.Windows.Automation.Peers
{
    public class RibbonGalleryItemDataAutomationPeer : ItemAutomationPeer, IScrollItemProvider, ISelectionItemProvider
    {
        #region constructor

        ///
        public RibbonGalleryItemDataAutomationPeer(object owner, ItemsControlAutomationPeer itemsControlAutomationPeer, RibbonGalleryCategoryDataAutomationPeer parentCategoryDataAutomationPeer)
            : base(owner, itemsControlAutomationPeer)
        {
            _parentCategoryDataAutomationPeer = parentCategoryDataAutomationPeer;
        }

        #endregion constructor

        #region AutomationPeer override

        ///
        override protected string GetClassNameCore()
        {
            return "RibbonGalleryItem";
        }

        ///
        override protected AutomationControlType GetAutomationControlTypeCore()
        {
            return AutomationControlType.ListItem;
        }

        ///
        override public object GetPattern(PatternInterface patternInterface)
        {
            if (patternInterface == PatternInterface.ScrollItem || patternInterface == PatternInterface.SelectionItem)
            {
                return this;
            }
            return null;
        }

        #endregion AutomationPeer override

        #region internal helper

        internal RibbonGalleryItem GetOwningRibbonGalleryItem()
        {
            RibbonGalleryItem owningRibbonGalleryItem = null;
            ItemsControlAutomationPeer itemsControlAutomationPeer = ItemsControlAutomationPeer;
            if (itemsControlAutomationPeer != null)
            {
                RibbonGalleryCategory containingCategory = (RibbonGalleryCategory)(itemsControlAutomationPeer.Owner);
                if (containingCategory != null)
                {
                    owningRibbonGalleryItem = (RibbonGalleryItem)containingCategory.ItemContainerGenerator.ContainerFromItem(Item);
                }
            }
            return owningRibbonGalleryItem;
        }

        #endregion 

        #region public properties

        public RibbonGalleryCategoryDataAutomationPeer ParentCategoryDataAutomationPeer
        {
            get
            {
                return _parentCategoryDataAutomationPeer;
            }
        }

        #endregion public properties

        #region IScrollItemProvider Members

        /// <summary>
        /// call wrapper.BringIntoView
        /// </summary>
        void IScrollItemProvider.ScrollIntoView()
        {
            RibbonGalleryItem ribbonGalleryItem = GetOwningRibbonGalleryItem();
            if (ribbonGalleryItem != null)
            {
                ribbonGalleryItem.BringIntoView();
            }
        }

        #endregion

        #region ISelectionItemProvider Members

        void ISelectionItemProvider.AddToSelection()
        {
            throw new InvalidOperationException();
        }

        bool ISelectionItemProvider.IsSelected
        {
            get 
            {
                RibbonGalleryItem ribbonGalleryItem = GetOwningRibbonGalleryItem();
                if (ribbonGalleryItem != null)
                {
                    return ribbonGalleryItem.IsSelected;
                }
                else
                {
                    throw new ElementNotAvailableException();
                }
            }
        }

        void ISelectionItemProvider.RemoveFromSelection()
        {
            RibbonGalleryItem ribbonGalleryItem = GetOwningRibbonGalleryItem();
            if (ribbonGalleryItem != null)
            {
                ribbonGalleryItem.IsSelected = false;
            }
            else
                throw new ElementNotAvailableException();
        }

        void ISelectionItemProvider.Select()
        {
            RibbonGalleryItem ribbonGalleryItem = GetOwningRibbonGalleryItem();
            if (ribbonGalleryItem != null)
            {
                ribbonGalleryItem.IsSelected = true;
            }
            else
                throw new ElementNotAvailableException();
        }

        IRawElementProviderSimple ISelectionItemProvider.SelectionContainer
        {
            get
            {
                RibbonGalleryCategoryDataAutomationPeer categoryDataPeer = ParentCategoryDataAutomationPeer;
                if(categoryDataPeer != null)
                {
                    RibbonGalleryAutomationPeer galleryAutomationPeer = categoryDataPeer.GetParent() as RibbonGalleryAutomationPeer;
                    if (galleryAutomationPeer != null)
                        return ProviderFromPeer(galleryAutomationPeer);
                }

                return null;
            }
        }
        
        #endregion

        #region data

        RibbonGalleryCategoryDataAutomationPeer _parentCategoryDataAutomationPeer;

        #endregion
    }
}
