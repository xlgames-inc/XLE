//---------------------------------------------------------------------------
// <copyright file="RibbonFilterMenuButton.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System;
using System.Windows;

namespace Microsoft.Windows.Controls.Ribbon
{
    /// <summary>
    ///   The filter menu button in a RibbonGallery has a different default Style (with a much different Template) than
    ///   a normal RibbonMenuButton.  Therefore, we've decided to create a new class for it so that we can specify our
    ///   Style as the default.
    ///
    ///   This way, if the user specifies RibbonGallery.FilterMenuButtonStyle, the supplied Style is merged with our
    ///   default Style instead of throwing it away.  Thus, app authors don't have to retemplate the filter menu button
    ///   if they just want to restyle a few properties.
    /// </summary>
    public class RibbonFilterMenuButton : RibbonMenuButton
    {
        #region Constructor
  
        /// <summary>
        ///   Initializes static members of the RibbonFilterMenuButton class.  Here we override the default Style.
        /// </summary>
        static RibbonFilterMenuButton()
        {
            Type ownerType = typeof(RibbonFilterMenuButton);
            DefaultStyleKeyProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(ownerType));
            RibbonControlService.CanAddToQuickAccessToolBarDirectlyProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(false));
        }
        
        #endregion

        #region DismissPopup

        bool _retainFocusOnDismiss = false;
        internal override void OnIsDropDownOpenChanged(DependencyPropertyChangedEventArgs e)
        {
            base.OnIsDropDownOpenChanged(e);
            if (IsDropDownOpen)
            {
                _retainFocusOnDismiss = RibbonHelper.IsKeyboardMostRecentInputDevice();
            }
        }

        internal override void OnAnyMouseDown(System.Windows.Input.MouseButtonEventArgs e)
        {
            base.OnAnyMouseDown(e);
            _retainFocusOnDismiss = false;
        }

        protected override void OnDismissPopup(RibbonDismissPopupEventArgs e)
        {
            base.OnDismissPopup(e);

            if (e.DismissMode == RibbonDismissPopupMode.Always)
            {
                // DismissPopup in RibbonFilterMenuButton shouldn't dismiss the parent Popup.
                // Retain focus on self if needed.
                if (_retainFocusOnDismiss)
                {
                    Focus();
                    _retainFocusOnDismiss = false;
                }
                e.Handled = true;
            }
        }

        #endregion

        #region KeyTips

        protected override void OnActivatingKeyTip(ActivatingKeyTipEventArgs e)
        {
            if (e.OriginalSource == this)
            {
                e.KeyTipHorizontalPlacement = KeyTipHorizontalPlacement.KeyTipCenterAtTargetCenter;
                e.KeyTipVerticalPlacement = KeyTipVerticalPlacement.KeyTipCenterAtTargetCenter;
                e.KeyTipHorizontalOffset = e.KeyTipVerticalOffset = 0;
            }
        }

        #endregion
    }
}
