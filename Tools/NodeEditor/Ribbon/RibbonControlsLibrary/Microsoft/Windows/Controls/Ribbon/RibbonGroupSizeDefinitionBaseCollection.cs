//---------------------------------------------------------------------------
// <copyright file="RibbonGroupSizeDefinitionCollection.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    #region Using declarations

    using System.Collections.Specialized;
    using System.Diagnostics;
    using System.Windows;

    #endregion

    /// <summary>
    ///   Provides a familiar name for XAML usage for a collection of RibbonGroupSizeDefinitions.
    /// </summary>
    public class RibbonGroupSizeDefinitionBaseCollection : FreezableCollection<RibbonGroupSizeDefinitionBase>
    {
        #region Freezable

        protected override Freezable CreateInstanceCore()
        {
            return new RibbonGroupSizeDefinitionBaseCollection();
        }

        #endregion
    }
}