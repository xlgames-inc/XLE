//---------------------------------------------------------------------------
// <copyright file="RibbonButtonAutomationPeer.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Automation.Peers
{
    #region Using declarations

    using System;
    using System.Windows;
    using System.Windows.Automation;
    using System.Windows.Automation.Peers;
    using System.Windows.Automation.Provider;
    using System.Windows.Controls;
    using Microsoft.Windows.Controls;
    using Microsoft.Windows.Controls.Ribbon;

    #endregion

    /// <summary>
    ///   An automation peer class which automates RibbonMenuItem control.
    /// </summary>
    public class RibbonMenuItemDataAutomationPeer : ItemAutomationPeer, IExpandCollapseProvider, IInvokeProvider, IToggleProvider, ITransformProvider
    {

        #region Constructors

        public RibbonMenuItemDataAutomationPeer(object item, ItemsControlAutomationPeer itemsControlPeer)
            : base(item, itemsControlPeer)
        {
        }

        #endregion

        #region AutomationPeer overrides

        /// <summary>
        ///   Return class name for automation clients to display
        /// </summary> 
        protected override string GetClassNameCore()
        {
            AutomationPeer wrapperPeer = GetWrapperPeer();
            if (wrapperPeer != null)
            {
                return wrapperPeer.GetClassName();
            }
            
            return "RibbonMenuItem";
        }

        protected override AutomationControlType GetAutomationControlTypeCore()
        {
            AutomationPeer wrapperPeer = GetWrapperPeer();
            if (wrapperPeer != null)
            {
                return wrapperPeer.GetAutomationControlType();
            }

            return AutomationControlType.MenuItem;
        }

        public override object GetPattern(PatternInterface patternInterface)
        {
            object result = null;
            FrameworkElement owner = GetWrapper();
            if (owner != null)
            {
                RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
                if (menuItemOwner == null)
                {
                    AutomationPeer wrapperPeer = GetWrapperPeer();
                    if (wrapperPeer != null)
                    {
                        result = wrapperPeer.GetPattern(patternInterface);
                    }
                }
                else
                {
                    MenuItemRole role = menuItemOwner.Role;
                    if (patternInterface == PatternInterface.ExpandCollapse)
                    {
                        if ((role == MenuItemRole.TopLevelHeader || role == MenuItemRole.SubmenuHeader)
                            && menuItemOwner.HasItems)
                        {
                            result = this;
                        }
                    }
                    else if (patternInterface == PatternInterface.Toggle)
                    {
                        result = this;
                    }
                    else if (patternInterface == PatternInterface.Invoke)
                    {
                        if ((role == MenuItemRole.TopLevelItem || role == MenuItemRole.SubmenuItem)
                            && !menuItemOwner.HasItems)
                        {
                            result = this;
                        }
                    }
                    else if (patternInterface == PatternInterface.Transform)
                    {
                        if (menuItemOwner.IsSubmenuOpen && (menuItemOwner.CanUserResizeHorizontally || menuItemOwner.CanUserResizeVertically))
                        {
                            result = this;
                        }
                    }
                    else
                    {
                        AutomationPeer wrapperPeer = GetWrapperPeer();
                        if (wrapperPeer != null)
                        {
                            result = wrapperPeer.GetPattern(patternInterface);
                        }
                    }
                }
            }

            return result;
        }

        #endregion

        #region IExpandCollapseProvider Members

        void IExpandCollapseProvider.Expand()
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            FrameworkElement owner = GetWrapper();
            if (owner == null)
            {
                throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
            }

            RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
            if (menuItemOwner != null)
            {
                MenuItemRole role = menuItemOwner.Role;

                if ((role != MenuItemRole.TopLevelHeader && role != MenuItemRole.SubmenuHeader)
                    || !menuItemOwner.HasItems)
                {
                    throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
                }

                menuItemOwner.IsSubmenuOpen = true;
            }
            else
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }
        }

        ///
        void IExpandCollapseProvider.Collapse()
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            FrameworkElement owner = GetWrapper();
            if (owner == null)
            {
                throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
            }

            RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
            if (menuItemOwner != null)
            {
                MenuItemRole role = menuItemOwner.Role;

                if ((role != MenuItemRole.TopLevelHeader && role != MenuItemRole.SubmenuHeader)
                    || !menuItemOwner.HasItems)
                {
                    throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
                }

                menuItemOwner.IsSubmenuOpen = false;
            }
            else
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }
        }

        ///
        ExpandCollapseState IExpandCollapseProvider.ExpandCollapseState
        {
            get
            {
                FrameworkElement owner = GetWrapper();
                if (owner == null)
                {
                    throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
                }

                ExpandCollapseState result = ExpandCollapseState.Collapsed;

                RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
                if (menuItemOwner != null)
                {
                    MenuItemRole role = menuItemOwner.Role;

                    if (role == MenuItemRole.TopLevelItem || role == MenuItemRole.SubmenuItem || !menuItemOwner.HasItems)
                    {
                        result = ExpandCollapseState.LeafNode;
                    }
                    else if (menuItemOwner.IsSubmenuOpen)
                    {
                        result = ExpandCollapseState.Expanded;
                    }
                }
                else
                {
                    throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
                }

                return result;
            }
        }

        #endregion

        #region IInvokeProvider Members

        void IInvokeProvider.Invoke()
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            FrameworkElement owner = GetWrapper();
            if (owner == null)
            {
                throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
            }

            RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
            if (menuItemOwner == null)
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }

            menuItemOwner.ClickItemInternal();
        }

        #endregion

        #region IToggleProvider Members

        void IToggleProvider.Toggle()
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            FrameworkElement owner = GetWrapper();
            if (owner == null)
            {
                throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
            }

            RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
            if (menuItemOwner == null || !menuItemOwner.IsCheckable)
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }

            menuItemOwner.IsChecked = !menuItemOwner.IsChecked;
        }

        ///
        ToggleState IToggleProvider.ToggleState
        {
            get
            {
                FrameworkElement owner = GetWrapper();
                if (owner == null)
                {
                    throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
                }

                RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
                if (menuItemOwner == null)
                {
                    throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
                }

                return menuItemOwner.IsChecked ? ToggleState.On : ToggleState.Off;
            }
        }

        #endregion

        #region ITransformProvider Members

        bool ITransformProvider.CanMove
        {
            get { return false; }
        }

        bool ITransformProvider.CanResize
        {
            get 
            {
                RibbonMenuItem owner = GetWrapper() as RibbonMenuItem;
                if (owner != null)
                {
                    return IsEnabled() && (owner.CanUserResizeVertically || owner.CanUserResizeHorizontally);
                }

                return false;
            }
        }

        bool ITransformProvider.CanRotate
        {
            get { return false; }
        }

        void ITransformProvider.Move(double x, double y)
        {
            throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
        }

        void ITransformProvider.Resize(double width, double height)
        {
            if (!IsEnabled())
                throw new ElementNotEnabledException();

            FrameworkElement owner = GetWrapper();
            if (owner == null)
            {
                throw new ElementNotAvailableException(SR.Get(SRID.VirtualizedElement));
            }

            RibbonMenuItem menuItemOwner = owner as RibbonMenuItem;
            if (menuItemOwner == null)
            {
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
            }

            if (!((ITransformProvider)this).CanResize || width <= 0 || height <= 0)
                throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));

            if (!menuItemOwner.ResizePopupInternal(width, height))
            {
                throw new InvalidOperationException(SR.Get(SRID.ResizeParametersNotValid));
            }
        }

        void ITransformProvider.Rotate(double degrees)
        {
            throw new InvalidOperationException(SR.Get(SRID.UIA_OperationCannotBePerformed));
        }

        #endregion

        #region Private Methods

        private AutomationPeer GetWrapperPeer()
        {
            AutomationPeer wrapperPeer = null;
            FrameworkElement wrapper = GetWrapper();
            if (wrapper != null)
            {
                wrapperPeer = UIElementAutomationPeer.CreatePeerForElement(wrapper);
            }
            return wrapperPeer;
        }

        private FrameworkElement GetWrapper()
        {
            ItemsControl parentItemsControl = (ItemsControl)ItemsControlAutomationPeer.Owner;
            return parentItemsControl.ItemContainerGenerator.ContainerFromItem(Item) as FrameworkElement;
        }

        #endregion

    }
}
