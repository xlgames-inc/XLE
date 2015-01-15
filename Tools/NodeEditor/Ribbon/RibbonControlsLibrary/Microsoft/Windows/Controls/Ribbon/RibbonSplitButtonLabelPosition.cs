//---------------------------------------------------------------------------
// <copyright file="RibbonSplitButtonLabelPosition.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows.Data;
using System;
using System.Globalization;
namespace Microsoft.Windows.Controls.Ribbon
{
    #region Using declarations
    #endregion

    /// <summary>
    ///   An enum that describes whether the RibbonSplitButton's label should be positioned
    ///   with the the 'Header' part or the 'DropDown' part 
    ///   when the RibbonSplitButton is in Medium variant.
    /// </summary>
    public enum RibbonSplitButtonLabelPosition
    {
        /// <summary>
        ///   Indicates that the label should be positioned with the
        ///   'Header' part of the RibbonSplitButton.
        /// </summary>
        Header,

        /// <summary>
        ///   Indicates that the label should be positioned with the
        ///   'DropDown' part of the RibbonSplitButton.
        /// </summary>
        DropDown,
    }
}
