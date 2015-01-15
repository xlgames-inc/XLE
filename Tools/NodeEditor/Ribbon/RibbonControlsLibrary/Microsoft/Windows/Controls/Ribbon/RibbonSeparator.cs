//---------------------------------------------------------------------------
// <copyright file="RibbonSeparator.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    #region Using declarations

    using System;
    using System.ComponentModel;
    using System.Windows;
    using System.Windows.Controls;
    using System.Windows.Data;
    using System.Windows.Media;

    #endregion Using declarations

    /// <summary>
    ///   A restyled Separator for the Ribbon.
    /// </summary>
    public class RibbonSeparator : Separator
    {
        #region Constructors

        /// <summary>
        ///   Initializes static members of the RibbonSeparator class.
        /// </summary>
        static RibbonSeparator()
        {
            Type ownerType = typeof(RibbonSeparator);

            DefaultStyleKeyProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(ownerType));
        }
        
        #endregion Constructors

        #region Public Properties

        /// <summary>
        ///     DependencyProperty for Label property.
        /// </summary>
        public static readonly DependencyProperty LabelProperty =
            RibbonControlService.LabelProperty.AddOwner(typeof(RibbonSeparator));

        /// <summary>
        ///     Primary label text for the control.
        /// </summary>
        public string Label
        {
            get { return RibbonControlService.GetLabel(this); }
            set { RibbonControlService.SetLabel(this, value); }
        }

        #endregion Public Properties

        #region VisualStates

        /// <summary>
        ///     DependencyProperty for Ribbon property.
        /// </summary>
        public static readonly DependencyProperty RibbonProperty =
            RibbonControlService.RibbonProperty.AddOwner(typeof(RibbonSeparator));

        /// <summary>
        ///     This property is used to access visual state brushes defined on the Ribbon class.
        /// </summary>
        public Ribbon Ribbon
        {
            get { return RibbonControlService.GetRibbon(this); }
        }

        internal object DefaultStyleKeyInternal
        {
            get { return DefaultStyleKey; }
            set { DefaultStyleKey = value; }
        }

        #endregion VisualStates

        #region SharedSizeScope

        /// <summary>
        ///     Called when the parent of the Visual has changed.
        /// </summary>
        /// <param name="oldParent">Old parent or null if the Visual did not have a parent before.</param>
        protected override void OnVisualParentChanged(DependencyObject oldParent)
        {
            base.OnVisualParentChanged(oldParent);

            // Windows OS bug:1988393; DevDiv bug:107459
            // RibbonMenuItem, RibbonMenuButton, RibbonSplitButton and RibbonSplitMenuItem 
            // templates contains ItemsPresenter where Grid.IsSharedSizeScope="true" and 
            // need to inherits PrivateSharedSizeScopeProperty value. Property inheritance 
            // walks the locial tree if possible and skips the visual tree where the 
            // ItemsPresenter is. Workaround here is to copy the property value from the 
            // visual parent

            DependencyObject newParent = VisualTreeHelper.GetParent(this);

            // The Separtator is a logical child of a RibbonMenuItem, RibbonMenuButton, 
            // RibbonSplitButton or RibbonSplitMenuItem but has a different visual parent. 
            // Set one-way binding with visual parent for 
            // DefinitionBase.PrivateSharedSizeScopeProperty
            if (newParent != null)
            {
                Binding binding = new Binding();
                binding.Path = new PropertyPath(PrivateSharedSizeScopeProperty);
                binding.Mode = BindingMode.OneWay;
                binding.Source = newParent;
                BindingOperations.SetBinding(this, PrivateSharedSizeScopeProperty, binding);
            }

            // Clear binding for DefinitionBase.PrivateSharedSizeScopeProperty as it 
            // is being detached from a visual parent.
            if (newParent == null)
            {
                BindingOperations.ClearBinding(this, PrivateSharedSizeScopeProperty);
            }

        }

        protected override void OnPropertyChanged(DependencyPropertyChangedEventArgs e)
        {
            base.OnPropertyChanged(e);

            // See OnVisualParentChanged method for the elaborate explanation. 
            // In a gist this is a hack to capture a reference to the 
            // PrivateSharedSizeScopeProperty.

            if (_privateSharedSizeScopeProperty == null &&
                e.Property.OwnerType == typeof(DefinitionBase) &&
                e.Property.Name == "PrivateSharedSizeScope")
            {
                _privateSharedSizeScopeProperty = e.Property;
            }
        }

        private DependencyProperty PrivateSharedSizeScopeProperty
        {
            get
            {
                if (_privateSharedSizeScopeProperty == null)
                {
                    // See OnVisualParentChanged method for the elaborate explanation. 
                    // In a gist this is a hack to trigger a PropertyChanged 
                    // notification for the PrivateSharedSizeScopeProperty.

                    SetValue(Grid.IsSharedSizeScopeProperty, true);
                    ClearValue(Grid.IsSharedSizeScopeProperty);
                }

                return _privateSharedSizeScopeProperty;
            }
        }

        private static DependencyProperty _privateSharedSizeScopeProperty;

        #endregion SharedSizeScope

        #region Protected Methods

        protected override System.Windows.Automation.Peers.AutomationPeer OnCreateAutomationPeer()
        {
            return new Microsoft.Windows.Automation.Peers.RibbonSeparatorAutomationPeer(this);
        }

        #endregion Protected Methods
    }
}
