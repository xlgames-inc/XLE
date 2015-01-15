//---------------------------------------------------------------------------
// <copyright file="RibbonCommands.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows.Input;

namespace Microsoft.Windows.Controls.Ribbon
{
    public static class RibbonCommands
    {
        public static RoutedUICommand AddToQuickAccessToolBarCommand { get; private set; }
        public static RoutedUICommand RemoveFromQuickAccessToolBarCommand { get; private set; }
        public static RoutedUICommand MinimizeRibbonCommand { get; private set; }
        public static RoutedUICommand MaximizeRibbonCommand { get; private set; }
        public static RoutedUICommand ShowQuickAccessToolBarAboveRibbonCommand { get; private set; }
        public static RoutedUICommand ShowQuickAccessToolBarBelowRibbonCommand { get; private set; }

        static RibbonCommands()
        {
            AddToQuickAccessToolBarCommand = new RoutedUICommand(RibbonContextMenu.AddToQATText, "AddToQuickAccessToolBar", typeof(RibbonCommands));
            RemoveFromQuickAccessToolBarCommand = new RoutedUICommand(RibbonContextMenu.RemoveFromQATText, "RemoveFromQuickAccessToolBar", typeof(RibbonCommands));
            MinimizeRibbonCommand = new RoutedUICommand(RibbonContextMenu.MinimizeTheRibbonText, "MinimizeRibbon", typeof(RibbonCommands));
            MaximizeRibbonCommand = new RoutedUICommand(RibbonContextMenu.MaximizeTheRibbonText, "MaximizeRibbon", typeof(RibbonCommands));
            ShowQuickAccessToolBarAboveRibbonCommand = new RoutedUICommand(RibbonContextMenu.ShowQATAboveText, "ShowQuickAccessToolBarAboveRibbon", typeof(RibbonCommands));
            ShowQuickAccessToolBarBelowRibbonCommand = new RoutedUICommand(RibbonContextMenu.ShowQATBelowText, "ShowQuickAccessToolBarBelowRibbon", typeof(RibbonCommands));
        }
    }
}
