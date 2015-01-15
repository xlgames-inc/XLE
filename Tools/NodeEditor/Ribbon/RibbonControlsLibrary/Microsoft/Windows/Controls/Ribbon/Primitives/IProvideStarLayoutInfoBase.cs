//---------------------------------------------------------------------------
// <copyright file="IProvideStarLayoutInfoBase.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows;

namespace Microsoft.Windows.Controls.Ribbon.Primitives
{
    /// <summary>
    ///     The interface is the star layout contract which provides
    ///     the data needed to do the star layout.
    /// </summary>
    public interface IProvideStarLayoutInfoBase
    {
        /// <summary>
        ///     The callback method which gets called every time before
        ///     the star allocator (ISupportStarLayout) gets measured
        /// </summary>
        void OnInitializeLayout();

        /// <summary>
        ///     The UIElement which this provider targets.
        /// </summary>
        UIElement TargetElement { get; }
    }
}