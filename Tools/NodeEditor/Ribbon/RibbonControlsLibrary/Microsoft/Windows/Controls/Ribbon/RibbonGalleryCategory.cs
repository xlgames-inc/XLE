//---------------------------------------------------------------------------
// <copyright file="RibbonGalleryCategory.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

namespace Microsoft.Windows.Controls.Ribbon
{
    #region Using declarations

    using System;
    using System.Windows;
    using System.Collections;
    using System.Collections.Specialized;
    using System.Collections.ObjectModel;
    using System.Windows.Controls;
    using System.Windows.Controls.Primitives;
    using System.Windows.Data;
    using System.Windows.Input;
    using System.Windows.Media;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Windows.Automation.Peers;
    using Microsoft.Windows.Automation.Peers;
    using Microsoft.Windows.Controls.Ribbon.Primitives;

    #endregion

    /// <summary>
    ///   RibbonGalleryCategory inherits from HeaderedItemsControl as it has Header and contains RibbonGalleryItems
    /// </summary>

    [StyleTypedProperty(Property = "ItemContainerStyle", StyleTargetType = typeof(RibbonGalleryItem))]
    [TemplatePart(Name = ItemsHostName, Type = typeof(ItemsPresenter))]
    [TemplatePart(Name = HeaderPresenterName, Type = typeof(ContentPresenter))]
    public class RibbonGalleryCategory : HeaderedItemsControl, IWeakEventListener
    {
        #region Constructors

        /// <summary>
        ///   Initializes static members of the RibbonGalleryCategory class.  
        /// </summary>
        static RibbonGalleryCategory()
        {
            Type ownerType = typeof(RibbonGalleryCategory);
            DefaultStyleKeyProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(ownerType));
            ItemTemplateProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(null, new PropertyChangedCallback(OnNotifyPropertyChanged), new CoerceValueCallback(CoerceItemTemplate)));
            ItemContainerStyleProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(new PropertyChangedCallback(OnNotifyPropertyChanged), new CoerceValueCallback(CoerceItemContainerStyle)));
            
            FocusableProperty.OverrideMetadata(ownerType, new FrameworkPropertyMetadata(false));
        }

        /// <summary>
        ///   Initializes an instance of the RibbonGalleryCategory class.
        /// </summary>
        public RibbonGalleryCategory()
        {
            this.ItemContainerGenerator.StatusChanged += new EventHandler(OnItemContainerGeneratorStatusChanged);
        }

        #endregion Constructors

        #region ContainerGeneration

        protected override bool IsItemItsOwnContainerOverride(object item)
        {
            return item is RibbonGalleryItem;
        }

        protected override DependencyObject GetContainerForItemOverride()
        {
            return new RibbonGalleryItem();
        }

        /// <summary>
        ///   Called when the container is being attached to the parent ItemsControl
        /// </summary>
        /// <param name="element"></param>
        /// <param name="item"></param>
        protected override void PrepareContainerForItemOverride(DependencyObject element, object item)
        {
            RibbonGalleryItem galleryItem = (RibbonGalleryItem)element;
            galleryItem.RibbonGalleryCategory = this;

            RibbonGallery gallery = RibbonGallery;
            if (gallery != null)
            {
                object selectedItem = gallery.SelectedItem;
                if (selectedItem != null)
                {
                    // Set IsSelected to true on GalleryItems that match the SelectedItem
                    if (RibbonGallery.VerifyEqual(item, selectedItem))
                    {
                        galleryItem.IsSelected = true;
                    }
                }
                else if (galleryItem.IsSelected)
                {
                    // If a GalleryItem is marked IsSelected true then synchronize SelectedItem with it
                    gallery.SelectedItem = item;
                }
                else
                {
                    object selectedValue = gallery.SelectedValue;
                    if (selectedValue != null)
                    {
                        // Set SelectedItem if the item's value matches the SelectedValue
                        object itemValue = gallery.GetSelectableValueFromItem(item);
                        if (RibbonGallery.VerifyEqual(selectedValue, itemValue))
                        {
                            galleryItem.IsSelected = true;
                        }
                    }
                }
            }

            galleryItem.SyncKeyTipAndContent();

            base.PrepareContainerForItemOverride(element, item);
        }

        /// <summary>
        ///   Called when the container is being detached from the parent ItemsControl
        /// </summary>
        /// <param name="element"></param>
        /// <param name="item"></param>
        protected override void ClearContainerForItemOverride(DependencyObject element, object item)
        {
            RibbonGalleryItem galleryItem = (RibbonGalleryItem)element;

            // Turn off selection and highlight on GalleryItems that are being cleared. 
            // Note that we directly call Change[Selection/Highlight] instead of setting 
            // Is[Selected/Highlighted] because we aren't able to get ItemFromContainer 
            // in OnIs[Selected/Highlighted]Changed because the ItemContainerGenerator 
            // has already detached this container. 
            if (galleryItem.IsHighlighted)
            {
                galleryItem.RibbonGallery.ChangeHighlight(item, galleryItem, false);
            }
            if (galleryItem.IsSelected)
            {
                galleryItem.RibbonGallery.ChangeSelection(item, galleryItem, false);
            }

            galleryItem.RibbonGalleryCategory = null;
            base.ClearContainerForItemOverride(element, item);
        }

        #endregion ContainerGeneration

        #region Tree

        internal RibbonGallery RibbonGallery
        {
            get;
            set;
        }

        // To fetch defined ItemsPanel in RibbonGalleryCategory's Template. It is set during ApplyTemplate.
        internal Panel ItemsHostSite
        {
            get;
            private set;
        }

        internal double MaxColumnWidth
        {
            get
            {
                RibbonGalleryItemsPanel galleryItemsPanel = ItemsHostSite as RibbonGalleryItemsPanel;
                if (galleryItemsPanel != null)
                {
                    return galleryItemsPanel.MaxColumnWidth;
                }

                return 0.0;
            }
        }

        #endregion Tree

        #region Template

        // The Panel is not ready during ApplyTemplate of the RibbonGallerryCategory, so it should be queried at later time.
        private void OnItemContainerGeneratorStatusChanged(object sender, EventArgs e)
        {
            if (ItemContainerGenerator.Status == GeneratorStatus.ContainersGenerated)
            {
                SynchronizeWithCurrentItem();

                if (_itemsPresenter != null)
                {
                    ItemsHostSite = (Panel)(ItemsPanel.FindName(RibbonGalleryCategory.ItemsHostPanelName, _itemsPresenter));
                }
            }
        }
        
        public override void OnApplyTemplate()
        {
            base.OnApplyTemplate();
            _itemsPresenter = (ItemsPresenter)GetTemplateChild(ItemsHostName);
            _headerPresenter = (ContentPresenter)GetTemplateChild(HeaderPresenterName);
            SyncProperties();
        }

        private static void OnNotifyPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            RibbonGalleryCategory galleryCategory = (RibbonGalleryCategory)d;
            galleryCategory.NotifyPropertyChanged(e);
            
            // This is for the layout related properties MinColumnCount/MaxColumnCount IsSharedColumnScope 
            // and MaxColumnWidth on RibbonGallery/RibbonGalleryCategory. This calls InvalidateMeasure 
            // for all the categories' ItemsPanel and they must use changed values.
            if (e.Property == MinColumnCountProperty || e.Property == MaxColumnCountProperty || e.Property == IsSharedColumnSizeScopeProperty || e.Property == ColumnsStretchToFillProperty)
            {
                RibbonGallery gallery = galleryCategory.RibbonGallery;
                if (gallery != null)
                {
                    gallery.InvalidateMeasureOnAllCategoriesPanel();
                }
            }
        }
        internal void NotifyPropertyChanged(DependencyPropertyChangedEventArgs e)
        {
            if (e.Property == ItemTemplateProperty || e.Property == RibbonGallery.GalleryItemTemplateProperty)
            {
                PropertyHelper.TransferProperty(this, ItemTemplateProperty);
            }
            else if (e.Property == ItemContainerStyleProperty || e.Property == RibbonGallery.GalleryItemStyleProperty)
            {
                PropertyHelper.TransferProperty(this, ItemContainerStyleProperty);
            }
            else if (e.Property == MinColumnCountProperty)
            {
                PropertyHelper.TransferProperty(this, MinColumnCountProperty);
            }
            else if (e.Property == MaxColumnCountProperty) 
            {
                PropertyHelper.TransferProperty(this, MaxColumnCountProperty);
            }
        }

        private static object CoerceItemTemplate(DependencyObject d, object baseValue)
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)d;
            return PropertyHelper.GetCoercedTransferPropertyValue(category,
                baseValue,
                ItemsControl.ItemTemplateProperty,
                category.RibbonGallery,
                RibbonGallery.GalleryItemTemplateProperty);
        }

        private static object CoerceItemContainerStyle(DependencyObject d, object baseValue)
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)d;
            return PropertyHelper.GetCoercedTransferPropertyValue(category,
                baseValue,
                ItemsControl.ItemContainerStyleProperty,
                category.RibbonGallery,
                RibbonGallery.GalleryItemStyleProperty);
        }

        // coerce the properties
        internal void SyncProperties()
        {
            PropertyHelper.TransferProperty(this, ItemTemplateProperty);
            PropertyHelper.TransferProperty(this, ItemContainerStyleProperty);
            PropertyHelper.TransferProperty(this, MinColumnCountProperty);
            PropertyHelper.TransferProperty(this, MaxColumnCountProperty);
        }

        #endregion Template

        #region Layout

        /// <summary>
        /// MinColumnCount is the property defined on RibbonGallery. RibbonGalleryCategory Adds
        /// itself Owner to this property.
        /// It's used by RibbonGalleryItemsPanel during Measure/Arrange which is default panel for RibbonGalleryCategory
        /// Default is 0
        /// </summary>
        public int MinColumnCount
        {
            get { return (int)GetValue(MinColumnCountProperty); }
            set { SetValue(MinColumnCountProperty, value); }
        }

        /// <summary>
        /// MinColumnCount is the property defined on RibbonGallery. RibbonGalleryCategory Adds
        /// itself Owner to this property.
        /// It's used by RibbonGalleryItemsPanel during Measure/Arrange which is default panel for RibbonGalleryCategory
        /// Default is 0
        /// </summary>
        public static readonly DependencyProperty MinColumnCountProperty = RibbonGallery.MinColumnCountProperty.AddOwner(typeof(RibbonGalleryCategory), new FrameworkPropertyMetadata(1, FrameworkPropertyMetadataOptions.AffectsMeasure, new PropertyChangedCallback(OnNotifyPropertyChanged), new CoerceValueCallback(CoerceMinColumnCount)));

        // Coerce MinColumnCount to retrieve the value defined on parent Gallery if it's not set here locally.
        private static object CoerceMinColumnCount(DependencyObject d, object baseValue)
        {
            RibbonGalleryCategory me = (RibbonGalleryCategory)d;
            return PropertyHelper.GetCoercedTransferPropertyValue(
                me,
                baseValue,
                MinColumnCountProperty,
                me.RibbonGallery,
                RibbonGallery.MinColumnCountProperty);
        }

        /// <summary>
        /// MaxColumnCount is the property defined on RibbonGallery. RibbonGalleryCategory Adds
        /// itself Owner to this property.
        /// It's used by RibbonGalleryItemsPanel during Measure/Arrange which is default panel for RibbonGalleryCategory
        /// Default is int.MaxValue
        /// </summary>
        public int MaxColumnCount
        {
            get { return (int)GetValue(MaxColumnCountProperty); }
            set { SetValue(MaxColumnCountProperty, value); }
        }

        /// <summary>
        /// MaxColumnCount is the property defined on RibbonGallery. RibbonGalleryCategory Adds
        /// itself Owner to this property.
        /// It's used by RibbonGalleryItemsPanel during Measure/Arrange which is default panel for RibbonGalleryCategory
        /// Default is int.MaxValue
        /// </summary>
        public static readonly DependencyProperty MaxColumnCountProperty = RibbonGallery.MaxColumnCountProperty.AddOwner(typeof(RibbonGalleryCategory), new FrameworkPropertyMetadata(int.MaxValue, FrameworkPropertyMetadataOptions.AffectsMeasure, new PropertyChangedCallback(OnNotifyPropertyChanged), new CoerceValueCallback(CoerceMaxColumnCount)));

        // Coerce Max   ColumnCount to retrieve the value defined on parent Gallery if it's not set here locally.
        private static object CoerceMaxColumnCount(DependencyObject d, object baseValue)
        {
            RibbonGalleryCategory me = (RibbonGalleryCategory)d;
            int maxColumnCount = (int)PropertyHelper.GetCoercedTransferPropertyValue(
                me,
                baseValue,
                MaxColumnCountProperty,
                me.RibbonGallery,
                RibbonGallery.MaxColumnCountProperty);
            
            if (maxColumnCount < (int)me.MinColumnCount)
                return (int)me.MinColumnCount;
            
            return maxColumnCount;
        }


        /// <summary>
        /// When ColumnsStretchToFill is true, RibbonGalleryItems are stretched during layout to occupy all the width available. 
        /// ColumnsStretchToFill is honored only when IsSharedColumnSizeScope is true.
        /// </summary>
        public bool ColumnsStretchToFill
        {
            get { return (bool)GetValue(ColumnsStretchToFillProperty); }
            set { SetValue(ColumnsStretchToFillProperty, value); }
        }

        public static readonly DependencyProperty ColumnsStretchToFillProperty = RibbonGallery.ColumnsStretchToFillProperty.AddOwner(typeof(RibbonGalleryCategory),
                                                                                                        new FrameworkPropertyMetadata(false, new PropertyChangedCallback(OnNotifyPropertyChanged)));

        /// <summary>
        ///     IsSharedColumnSizeScope: defined on RibbonGallery. RibbonGalleryCategory adds itself owner to it.
        ///     It's a BooleanProperty where True means that I(the control on which property is set) am the Scope for 
        ///     Uniform layout of items. Truth Table for this could be defined by:
        ///     (gallery.IsSharedColumnScope && !category.ISSharedColumnScope)? Gallery Scope  : Category Scope
        ///     Default Scope must result as Gallery scope hence default value on Gallery is True and on Category it's false. 
        /// </summary>
        public bool IsSharedColumnSizeScope 
        {
            get { return (bool)GetValue(IsSharedColumnSizeScopeProperty); }
            set { SetValue(IsSharedColumnSizeScopeProperty, value); }
        }

        /// <summary>
        ///     IsSharedColumnSizeScope: defined on RibbonGallery. RibbonGalleryCategory adds itself owner to it.
        ///     It's a BooleanProperty where True means that I(the control on which property is set) am the Scope for 
        ///     Uniform layout of items. Truth Table for this could be defined by:
        ///     (gallery.IsSharedColumnScope && !category.ISSharedColumnScope)? Gallery Scope  : Category Scope
        ///     Default Scope must result as Gallery scope hence default value on Gallery is True and on Category it's false. 
        /// </summary>
        public static readonly DependencyProperty IsSharedColumnSizeScopeProperty = RibbonGallery.IsSharedColumnSizeScopeProperty.AddOwner(typeof(RibbonGalleryCategory), new FrameworkPropertyMetadata(false, FrameworkPropertyMetadataOptions.AffectsMeasure, new PropertyChangedCallback(OnNotifyPropertyChanged)));

        #endregion Layout

        #region Selection

        protected override void OnInitialized(EventArgs e)
        {
            base.OnInitialized(e);
            AddCurrentItemChangedListener();
        }

        protected override void OnItemsSourceChanged(IEnumerable oldValue, IEnumerable newValue)
        {
            base.OnItemsSourceChanged(oldValue, newValue);
            RemoveCurrentItemChangedListener();
            AddCurrentItemChangedListener();

            // Note that it is possible that we haven't had a chance to sync 
            // to the CurrentItem on the CollectionView yet. This could happen, 
            // if the ItemContainers were generated synchronously and the base 
            // implementation fired ItemContainerGeneratorStatusChanged before 
            // we got here. So this is one more attempt to keep things in sync.

            SynchronizeWithCurrentItem();
        }

        internal void AddCurrentItemChangedListener()
        {
            if (RibbonGallery != null && RibbonGallery.IsSynchronizedWithCurrentItemInternal)
            {
                CollectionView = Items;

                // Listen for currency changes on the immediate CollectionView for the Category
                CurrentChangedEventManager.AddListener(CollectionView, this);
            }
        }

        internal void RemoveCurrentItemChangedListener()
        {
            // Stop listening for currency changes on the immediate CollectionView for the Category
            if (CollectionView != null)
            {
                CurrentChangedEventManager.RemoveListener(CollectionView, this);
                CollectionView = null;
            }
        }

        internal CollectionView CollectionView
        {
            get;
            set;
        }

        bool IWeakEventListener.ReceiveWeakEvent(Type managerType, object sender, EventArgs e)
        {
            if (managerType == typeof(CurrentChangedEventManager))
            {
                // Update currency on the immediate CollectionView for the Category
                OnCurrentItemChanged();
            }
            else
            {
                // Unrecognized event
                return false;
            }

            return true;
        }

        private void SynchronizeWithCurrentItem()
        {
            if (RibbonGallery != null && RibbonGallery.IsSynchronizedWithCurrentItemInternal)
            {
                if (ItemContainerGenerator.Status == GeneratorStatus.ContainersGenerated)
                {
                    if (RibbonGallery != null && RibbonGallery.SelectedItem == null)
                    {
                        // Since there isn't already a SelectedItem 
                        // synchronize it to match CurrentItem
                        OnCurrentItemChanged();
                    }
                }
            }
        }

        private void OnCurrentItemChanged()
        {
            Debug.Assert(RibbonGallery == null || RibbonGallery.IsSynchronizedWithCurrentItemInternal, "We shouldn't be listening for currency changes if IsSynchronizedWithCurrentItemInternal is false");

            if (RibbonGallery == null || CollectionView == null || RibbonGallery.IsSelectionChangeActive)
            {
                return;
            }

            RibbonGalleryItem galleryItem = this.ItemContainerGenerator.ContainerFromItem(CollectionView.CurrentItem) as RibbonGalleryItem;
            if (galleryItem != null)
            {
                // This is fast path to have the SelectedItem set
                galleryItem.IsSelected = true;
            }
            else
            {
                // This is to handle UI virtualization scenarios where the 
                // container for the CurrentItem may not have been generated.
                RibbonGallery.SelectedItem = CollectionView.CurrentItem;
            }
        }

        #endregion Selection

        #region CollectionChange

        /// <summary>
        ///     This method is invoked when the Items property changes.
        /// </summary>
        protected override void OnItemsChanged(NotifyCollectionChangedEventArgs e)
        {
            switch (e.Action)
            { 
                case NotifyCollectionChangedAction.Remove:
                case NotifyCollectionChangedAction.Replace:
                    if (RibbonGallery != null)
                    {
                        object selectedItem = RibbonGallery.SelectedItem;
                        object highlightedItem = RibbonGallery.HighlightedItem;
                        if (selectedItem != null || highlightedItem != null)
                        {
                            for (int i = 0; i < e.OldItems.Count; i++)
                            {
                                if (selectedItem != null && RibbonGallery.VerifyEqual(selectedItem, e.OldItems[i]))
                                {
                                    // Synchronize SelectedItem if it is one of 
                                    // the items being removed or replaced.
                                    RibbonGallery.ForceCoerceSelectedItem();
                                    break;
                                }
                                if (highlightedItem != null && RibbonGallery.VerifyEqual(highlightedItem, e.OldItems[i]))
                                {
                                    // Synchronize HighlightedItem if it is one of 
                                    // the items being removed or replaced.
                                    RibbonGallery.ForceCoerceHighlightedItem();
                                    break;
                                }
                            }
                        }
                        RibbonGallery.IsMaxColumnWidthValid = false;
                    }
                    break;
                case NotifyCollectionChangedAction.Reset:
                    if (RibbonGallery != null)
                    {
                        if (RibbonGallery.SelectedItem != null)
                        {
                            // Synchronize SelectedItem after a Reset operation.
                            RibbonGallery.ForceCoerceSelectedItem();
                        }
                        if (RibbonGallery.HighlightedItem != null)
                        {
                            // Synchronize HighlightedItem after a Reset operation.
                            RibbonGallery.ForceCoerceHighlightedItem();
                        }

                        RibbonGallery.IsMaxColumnWidthValid = false;
                    }
                    break;

                case NotifyCollectionChangedAction.Add:
                    if (RibbonGallery != null)
                    {
                        RibbonGallery.IsMaxColumnWidthValid = false;
                    }
                    break;
                case NotifyCollectionChangedAction.Move:
                    break;
            }
        }

        #endregion CollectionChange

        #region Automation

        ///
        protected override AutomationPeer OnCreateAutomationPeer()
        {
            return new RibbonGalleryCategoryAutomationPeer(this);
        }

        #endregion Automation

        #region CategoryHeader

        /// <summary>
        ///     Indicates whether this RibbonGalleryCategory's header visibility state.
        /// </summary>
        public static readonly DependencyProperty HeaderVisibilityProperty = 
                    DependencyProperty.Register(
                        "HeaderVisibility",
                        typeof(Visibility),
                        typeof(RibbonGalleryCategory),
                        new FrameworkPropertyMetadata(Visibility.Visible));

        /// <summary>
        ///     Indicates whether this RibbonGalleryCategory's header visibility state.
        /// </summary>
        public Visibility HeaderVisibility 
        {
            get { return (Visibility)GetValue(HeaderVisibilityProperty); }
            set { SetValue(HeaderVisibilityProperty, value); }
        }

        internal ContentPresenter HeaderPresenter
        {
            get { return _headerPresenter; }
        }

        #endregion CategoryHeader

        #region Data

        private const string ItemsHostPanelName = "ItemsHostPanel";
        private const string ItemsHostName = "ItemsHost";
        private const string HeaderPresenterName = "PART_Header";
        private ItemsPresenter _itemsPresenter;
        private ContentPresenter _headerPresenter;

        internal AverageItemHeightInfo averageItemHeightInfo;

        #endregion Data

        #region internal struct
        
        internal struct AverageItemHeightInfo
        {
            internal int count;
            internal double cumulativeHeight;
        }

        #endregion

    }
}
