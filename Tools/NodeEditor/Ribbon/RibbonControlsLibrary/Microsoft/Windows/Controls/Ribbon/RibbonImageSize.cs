//---------------------------------------------------------------------------
// <copyright file="RibbonImageSize.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    /// <summary>
    /// An enumeration of available image sizes.
    /// </summary>
    public enum RibbonImageSize
    {
        /// <summary>
        /// Indicates that the image should be collapsed
        /// </summary>
        Collapsed,

        /// <summary>
        /// Indicates that a small image should be used. (Usually 16x16 at 96dpi)
        /// </summary>
        Small,

        /// <summary>
        /// Indicates that a large image should be used. (Usually 32x32 at 96dpi)
        /// </summary>
        Large
    }
}