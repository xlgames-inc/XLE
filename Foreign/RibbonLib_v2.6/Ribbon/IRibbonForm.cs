//*****************************************************************************
//
//  File:       IRibbonForm.cs
//
//  Contents:   Definition of IRibbonForm interface. 
//              Each WinForm that wan to have a ribbon in it should implement 
//              this interface. 
//
//*****************************************************************************

using System;

namespace RibbonLib
{
    /// <summary>
    /// IRibbonForm interface should be implemented by the main form who want the ribbon
    /// </summary>
    public interface IRibbonForm
    {
        /// <summary>
        /// Getter for the main form window handle
        /// </summary>
        IntPtr WindowHandle { get; }

        /// <summary>
        /// Called when the ribbon height is changed, 
        /// allowing the form to reposition its controls so the ribbon doesn't hide them.
        /// </summary>
        /// <param name="newHeight">new height</param>
        void RibbonHeightUpdated(int newHeight);
    }
}
