//---------------------------------------------------------------------------
// <copyright file="IContainsStarLayoutManager.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System;

namespace Microsoft.Windows.Controls.Ribbon.Primitives
{
    internal interface IContainsStarLayoutManager
    {
        ISupportStarLayout StarLayoutManager
        {
            get;
            set;
        }
    }
}
