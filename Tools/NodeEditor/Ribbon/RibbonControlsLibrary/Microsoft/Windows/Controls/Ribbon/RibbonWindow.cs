//---------------------------------------------------------------------------
// <copyright file="RibbonWindow.cs" company="Microsoft Corporation">
//      Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    #region Using declarations

    using System;
    using System.Diagnostics;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Data;
    using System.Windows.Input;
    using System.Windows.Media;
    using Microsoft.Windows.Shell;

    #endregion

    /// <summary>
    ///   A Ribbon specific Window class which allos the Ribbon to draw onto
    ///   the non-client area to overdraw the application menu and contextual tab groups.
    /// </summary>
    [TemplatePart(Name = RibbonWindow._clientAreaBorderTemplateName, Type = typeof(Border))]
    [TemplatePart(Name = RibbonWindow._iconTemplateName, Type = typeof(Image))]
    public class RibbonWindow : Window
    {
        #region Constructors

        /// <summary>
        ///     Static constructor.
        ///     Initializes static members of the RibbonWindow class.
        /// </summary>
        static RibbonWindow()
        {
            Type ownerType = typeof(RibbonWindow);

            DefaultStyleKeyProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(new ComponentResourceKey(typeof(Ribbon), "RibbonWindowStyle")));

            // We override Window.Title metadata so that we can receive change notifications and then coerce Ribbon.Title.
            Window.TitleProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(String.Empty, new PropertyChangedCallback(OnTitleChangedCallback)));

            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(SystemCommands.MinimizeWindowCommand, MinimizeWindowExecuted, MinimizeWindowCanExecute));
            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(SystemCommands.MaximizeWindowCommand, MaximizeWindowExecuted, MaximizeWindowCanExecute));
            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(SystemCommands.RestoreWindowCommand, RestoreWindowExecuted, RestoreWindowCanExecute));
            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(SystemCommands.CloseWindowCommand, CloseWindowExecuted, CloseWindowCanExecute));
            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(SystemCommands.ShowSystemMenuCommand, SystemMenuExecuted, SystemMenuCanExecute));
            CommandManager.RegisterClassCommandBinding(ownerType, new CommandBinding(ApplicationCommands.Close, CloseApplicationExecuted, CloseApplicationCanExecute));
        }

        #endregion

        #region OnTitleChanged

        private static void OnTitleChangedCallback(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            RibbonWindow rw = d as RibbonWindow;
            rw.OnTitleChanged(null);
        }

        internal void OnTitleChanged(EventArgs e)
        {
            if (TitleChanged != null)
            {
                TitleChanged(this, e);
            }
        }

        #endregion

        #region Overrides

        public override void OnApplyTemplate()
        {
            base.OnApplyTemplate();

            // Hook up events to the system icon.
            _icon = GetTemplateChild(_iconTemplateName) as Image;

            if (_icon != null)
            {
                _icon.MouseLeftButtonDown += new MouseButtonEventHandler(IconMouseLeftButtonDown);
                _icon.MouseRightButtonDown += new MouseButtonEventHandler(IconMouseRightButtonDown);
            }

            _clientAreaBorder = GetTemplateChild(RibbonWindow._clientAreaBorderTemplateName) as Border;
        }

        #endregion

        #region Private Data

        /// <summary>
        ///     The Window Icon.
        /// </summary>
        private Image _icon;

        /// <summary>
        ///     The Border that hosts the client content of the RibbonWindow.  Also used to position the SystemMenu.
        /// </summary>
        private Border _clientAreaBorder;

        private const string _iconTemplateName = "PART_Icon";
        private const string _clientAreaBorderTemplateName = "PART_ClientAreaBorder";

        #endregion

        #region Event and Command Handlers

        internal event EventHandler TitleChanged;

        private static void MinimizeWindowCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null &&
                rw.WindowState != WindowState.Minimized)
            {
                args.CanExecute = true;
            }
        }

        private static void MinimizeWindowExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                SystemCommands.MinimizeWindow(rw);
                args.Handled = true;
            }
        }

        private static void MaximizeWindowCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null
                && rw.WindowState != WindowState.Maximized)
            {
                args.CanExecute = true;
            }
        }

        private static void MaximizeWindowExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                SystemCommands.MaximizeWindow(rw);
                args.Handled = true;
            }
        }

        private static void RestoreWindowCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null &&
                rw.WindowState != WindowState.Normal)
            {
                args.CanExecute = true;
            }
        }

        private static void RestoreWindowExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                SystemCommands.RestoreWindow(rw);
                args.Handled = true;
            }
        }

        private static void CloseWindowCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            args.CanExecute = true;
        }

        private static void CloseWindowExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                SystemCommands.CloseWindow(rw);
                args.Handled = true;
            }
        }

        private static void SystemMenuCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            args.CanExecute = true;
        }

        private static void SystemMenuExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                // For right-clicks, display the system menu from the point of the mouse click.
                // For left-clicks, display the system menu in the top-left corner of the client area.
                Point devicePoint;
                MouseButtonEventArgs e = args.Parameter as MouseButtonEventArgs;
                if (e != null)
                {
                    // This is the right-click handler.  The presence of a MouseButtonEventArgs as args.Parameter
                    // indicates we are handling right-click.
                    devicePoint = rw.PointToScreen(e.GetPosition(rw));
                }
                else if (rw._clientAreaBorder != null) 
                {
                    // This is the left-click handler.  We can only handle it correctly if the _clientAreaBorder
                    // template part is defined, because that is where we want to position the system menu.
                    devicePoint = rw._clientAreaBorder.PointToScreen(new Point(0, 0));
                }
                else
                {
                    // We can't handle this correctly, so exit.
                    return;
                }

                CompositionTarget compositionTarget = PresentationSource.FromVisual(rw).CompositionTarget;
                SystemCommands.ShowSystemMenu(rw, compositionTarget.TransformFromDevice.Transform(devicePoint));
                args.Handled = true;
            }
        }

        private static void CloseApplicationCanExecute(object sender, CanExecuteRoutedEventArgs args)
        {
            args.CanExecute = true;
        }

        private static void CloseApplicationExecuted(object sender, ExecutedRoutedEventArgs args)
        {
            RibbonWindow rw = sender as RibbonWindow;
            if (rw != null)
            {
                Application.Current.Shutdown();
                args.Handled = true;
            }
        }

        #endregion

        #region Internal Methods

        /// <summary>
        ///   This method allows Ribbon to propagate the WindowIconVisibility property to its containing RibbonWindow.
        /// </summary>
        internal void ChangeIconVisibility(Visibility newVisibility)
        {
            if (_icon != null)
            {
                _icon.Visibility = newVisibility;
            }
        }

        #endregion

        #region Private Methods

        /// <summary>
        ///   This handles the click events on the window icon.
        /// </summary>
        /// <param name="sender">Click event sender</param>
        /// <param name="e">event args</param>
        private void IconMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 1)
            {
                if (SystemCommands.ShowSystemMenuCommand.CanExecute(null, this))
                {
                    SystemCommands.ShowSystemMenuCommand.Execute(null, this);
                }
            }
            else if (e.ClickCount == 2)
            {
                if (ApplicationCommands.Close.CanExecute(null, this))
                {
                    ApplicationCommands.Close.Execute(null, this);
                }
            }
        }

        /// <summary>
        ///   This handles right-click events on the window icon.
        ///
        ///   For right-clicking, we want to display the system menu from the point of the mouse click
        ///   instead of from the top-left corner of the client area like we do with left clicks. So,
        ///   we pass the MouseButtonEventArgs to the SystemMenuExecuted handler.
        /// </summary>
        /// <param name="sender">Click event sender</param>
        /// <param name="e">event args</param>
        private void IconMouseRightButtonDown(object sender, MouseButtonEventArgs e)
        {
            if (SystemCommands.ShowSystemMenuCommand.CanExecute(e, this))
            {
                SystemCommands.ShowSystemMenuCommand.Execute(e, this);
            }
        }

        #endregion
    }
}
