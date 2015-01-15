//---------------------------------------------------------------------------
// <copyright file="ISupportStarLayout.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon.Primitives
{
    /// <summary>
    ///     Interface for the element which computes and allocates the
    ///     widths in the star layout contract.
    /// </summary>
    public interface ISupportStarLayout
    {
        /// <summary>
        ///     The method through which a provider (IProvideStarLayout) element
        ///     register itself to participate in the star layout.
        /// </summary>
        void RegisterStarLayoutProvider(IProvideStarLayoutInfoBase starLayoutInfoProvider);

        /// <summary>
        ///     The method through which a provider (IProvideStarLayout) element
        ///     unregisters itself from star layout participation.
        /// </summary>
        void UnregisterStarLayoutProvider(IProvideStarLayoutInfoBase starLayoutInfoProvider);

        /// <summary>
        ///     The property which lets proviers know whether the current layout (measure) pass
        ///     is after star allocation or not.
        /// </summary>
        bool IsStarLayoutPass { get; }
    }
}