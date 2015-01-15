//---------------------------------------------------------------------------
// <copyright file="KeyTipVerticalPlacement.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls
{
    /// <summary>
    ///     Enumeration for vertical placement of the keytip
    ///     with respect to its placement target.
    /// </summary>
    public enum KeyTipVerticalPlacement
    {
        KeyTipTopAtTargetTop = 0,
        KeyTipTopAtTargetCenter,
        KeyTipTopAtTargetBottom,
        KeyTipCenterAtTargetTop,
        KeyTipCenterAtTargetCenter,
        KeyTipCenterAtTargetBottom,
        KeyTipBottomAtTargetTop,
        KeyTipBottomAtTargetCenter,
        KeyTipBottomAtTargetBottom
    }
}