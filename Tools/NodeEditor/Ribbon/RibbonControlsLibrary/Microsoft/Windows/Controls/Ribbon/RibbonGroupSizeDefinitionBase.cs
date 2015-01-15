//---------------------------------------------------------------------------
// <copyright file="RibbonGroupSizeDefinitionBase.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows;

namespace Microsoft.Windows.Controls.Ribbon
{
    public abstract class RibbonGroupSizeDefinitionBase : Freezable
    {
        #region Public Properties

        public bool IsCollapsed
        {
            get { return (bool)GetValue(IsCollapsedProperty); }
            set { SetValue(IsCollapsedProperty, value); }
        }

        // Using a DependencyProperty as the backing store for IsCollapsed.  This enables animation, styling, binding, etc...
        public static readonly DependencyProperty IsCollapsedProperty =
            DependencyProperty.Register("IsCollapsed", typeof(bool), typeof(RibbonGroupSizeDefinitionBase), new FrameworkPropertyMetadata(false));

        #endregion
    }
}