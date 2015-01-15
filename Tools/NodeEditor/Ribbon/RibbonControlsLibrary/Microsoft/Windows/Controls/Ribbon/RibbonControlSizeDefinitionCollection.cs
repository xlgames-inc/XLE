//---------------------------------------------------------------------------
// <copyright file="RibbonControlSizeDefinitionCollection.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Collections.Specialized;
using System.Windows;

namespace Microsoft.Windows.Controls.Ribbon
{
    public class RibbonControlSizeDefinitionCollection : FreezableCollection<RibbonControlSizeDefinition>
    {
        #region Freezable

        protected override Freezable CreateInstanceCore()
        {
            return new RibbonControlSizeDefinitionCollection();
        }

        #endregion
    }
}