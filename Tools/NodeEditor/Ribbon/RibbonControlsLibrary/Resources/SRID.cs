//---------------------------------------------------------------------------
//
// Copyright (C) Microsoft Corporation.  All rights reserved.
//
//---------------------------------------------------------------------------

using System;
using System.Resources;
using System.Globalization;

namespace Microsoft.Windows.Controls
{
    // A wrapper around string identifiers.
    internal struct SRID
    {
        private string _string;

        public string String 
        { 
           get 
           { 
               return _string; 
           } 
        }

        private SRID(string s) 
        { 
           _string = s; 
        }

        // Ribbon
        public static SRID Ribbon_ContextualTabHeadersSourceInvalid { get { return new SRID("Ribbon_ContextualTabHeadersSourceInvalid"); } }

        // RibbonGallery
        public static SRID RibbonGallery_AllFilter { get { return new SRID("RibbonGallery_AllFilter"); } }

        // RibbonContextMenu
        public static SRID RibbonContextMenu_AddToQAT { get { return new SRID("RibbonContextMenu_AddToQAT"); } }
        public static SRID RibbonContextMenu_AddGalleryToQAT { get { return new SRID("RibbonContextMenu_AddGalleryToQAT"); } }
        public static SRID RibbonContextMenu_RemoveFromQAT { get { return new SRID("RibbonContextMenu_RemoveFromQAT"); } }
        public static SRID RibbonContextMenu_ShowQATAbove { get { return new SRID("RibbonContextMenu_ShowQATAbove"); } }
        public static SRID RibbonContextMenu_ShowQATBelow { get { return new SRID("RibbonContextMenu_ShowQATBelow"); } }
        public static SRID RibbonContextMenu_MaximizeTheRibbon { get { return new SRID("RibbonContextMenu_MaximizeTheRibbon"); } }
        public static SRID RibbonContextMenu_MinimizeTheRibbon { get { return new SRID("RibbonContextMenu_MinimizeTheRibbon"); } }

        // RibbonQuickAccessToolBar
        public static SRID RibbonQuickAccessToolBar_OverflowButtonToolTip { get { return new SRID("RibbonQuickAccessToolBar_OverflowButtonToolTip"); } }

        // UI Automation Exceptions
        public static SRID UIA_OperationCannotBePerformed { get { return new SRID("UIA_OperationCannotBePerformed"); } }
        public static SRID VirtualizedElement { get { return new SRID("VirtualizedElement"); } }
        public static SRID ResizeParametersNotValid { get { return new SRID("ResizeParametersNotValid"); } }
        public static SRID SetFocusFailed { get { return new SRID("SetFocusFailed"); } }

        // Misc layout
        public static SRID InvalidCtorParameterNoNaN { get { return new SRID("InvalidCtorParameterNoNaN"); } }
        public static SRID InvalidCtorParameterNoInfinityForStarSize { get { return new SRID("InvalidCtorParameterNoInfinityForStarSize"); } }
        public static SRID InvalidCtorParameterUnknownRibbonControlLengthUnitType { get { return new SRID("InvalidCtorParameterUnknownRibbonControlLengthUnitType"); } }
        public static SRID RibbonGroupsPanel_InvalidRegistrationParameter { get { return new SRID("RibbonGroupsPanel_InvalidRegistrationParameter"); } }
        public static SRID InvalidMenuButtonOrItemContainer { get { return new SRID("InvalidMenuButtonOrItemContainer"); } }
        public static SRID InvalidApplicationMenuOrItemContainer { get { return new SRID("InvalidApplicationMenuOrItemContainer"); } }

        // KeyTips
        public static SRID ElementNotKeyTipScope { get { return new SRID("ElementNotKeyTipScope"); } }
        public static SRID InvalidKeyTipOffset { get { return new SRID("InvalidKeyTipOffset"); } }
    }
}
