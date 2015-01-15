//---------------------------------------------------------------------------
// <copyright file="IProvideStarLayoutInfo.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Collections.Generic;

namespace Microsoft.Windows.Controls.Ribbon.Primitives
{
    /// <summary>
    ///     The interface is the star layout contract which provides
    ///     the data needed to do the star layout.
    /// </summary>
    public interface IProvideStarLayoutInfo : IProvideStarLayoutInfoBase
    {
        /// <summary>
        ///     The enumeration of spatially non-overlapping
        ///     star combinations.
        /// </summary>
        IEnumerable<StarLayoutInfo> StarLayoutCombinations { get; }

        /// <summary>
        ///     Method which gets called every time star allocated
        ///     is completed.
        /// </summary>
        void OnStarSizeAllocationCompleted();
    }
}