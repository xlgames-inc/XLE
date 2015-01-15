//---------------------------------------------------------------------------
// <copyright file="RibbonControlLengthUnitType.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    /// <summary>
    ///   Indicates what kind of value a RibbonControlLength is holding.
    /// </summary>
    /// <remarks>
    ///   Note: Keep the RibbonControlLengthUnitType enum in sync with the string representation 
    //       of units (RibbonControlLengthConverter._unitStrings). 
    /// </remarks>
    public enum RibbonControlLengthUnitType
    {
        Auto = 0,
        Pixel,
        Item,
        Star,
    }
}
