//---------------------------------------------------------------------------
// <copyright file="RibbonGalleryItemsPanel.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------
using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Diagnostics;
using Microsoft.Windows.Controls.Ribbon;
using Microsoft.Windows.Controls.Ribbon.Primitives;
using MS.Internal;

namespace Microsoft.Windows.Controls.Ribbon.Primitives
{
    /// <summary>
    ///   The default Items panel for the RibbonGallery class.
    /// </summary>
    public class RibbonGalleryItemsPanel : Panel
    {

        #region private methods

        // It just determines if any ancestor supports StarLayout and is not in StarLayoutPass mode.
        private bool IsAutoLayoutPass(double sumOfHeight, int childrenCount)
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                // Adding virtual count of items and cumulative height to RGC for the purpose of calcualting 
                // avg height as scrolling delta in RibbonGalleryCategoriesPanel.
                category.averageItemHeightInfo.count = childrenCount;
                category.averageItemHeightInfo.cumulativeHeight = sumOfHeight;
                RibbonGallery gallery = category.RibbonGallery;
                if (gallery != null)
                {
                    RibbonGalleryCategoriesPanel categoriesPanel = (RibbonGalleryCategoriesPanel)gallery.ItemsHostSite;
                    if (categoriesPanel != null)
                    {
                        IContainsStarLayoutManager iContainsStarLayoutManager = (IContainsStarLayoutManager)categoriesPanel;
                        if (iContainsStarLayoutManager.StarLayoutManager != null)
                            return !iContainsStarLayoutManager.StarLayoutManager.IsStarLayoutPass;
                    }
                }
            }

            return false;
        }

        // Minimum number of Col must be shown is determined by this method
        private int GetMinColumnCount()
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                return (int)category.MinColumnCount;
            }
            return 1;
        }

        // Maximum number of Col must be shown is determined by this method
        private int GetMaxColumnCount()
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                return (int)category.MaxColumnCount;
            }

            return int.MaxValue;
        }

        // Gets the MaxColumnWidth if the shared scope is Gallery (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
        // then MaxColumnWidth is used which is defined on Gallery to lay out all Items as Uniform width columns in scope of Gallery
        // otherwise they layout as Uniform width column within the scope of current Category only.
        private double GetMaxColumnWidth()
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                RibbonGallery gallery = category.RibbonGallery;
                if (gallery != null)
                {
                    if (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
                    {
                        return gallery.MaxColumnWidth;
                    }
                }
            }
            return _maxColumnWidth;
        }

        // Sets the MaxColumnWidth on Gallery if the shared scope is Gallery (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
        private void SetMaxColumnWidth(double value)
        {
            MaxColumnWidth = value;

            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                RibbonGallery gallery = category.RibbonGallery;
                if (gallery != null)
                {
                    if (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
                    {
                        if (gallery.MaxColumnWidth < value || !gallery.IsMaxColumnWidthValid)
                        {
                            gallery.MaxColumnWidth = value;
                            gallery.IsMaxColumnWidthValid = true;
                        }
                    }
                }
            }
        }

        private double GetArrangeWidth()
        {
            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                RibbonGallery gallery = category.RibbonGallery;
                if (gallery != null)
                {
                    if (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
                    {
                        return gallery.ArrangeWidth;
                    }
                }
            }
            return _arrangeWidth;
        }

        // Sets the ArrangeWidth on Gallery if the shared scope is Gallery (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
        private void SetArrangeWidth(double value)
        {
            _arrangeWidth = value;

            RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            if (category != null)
            {
                RibbonGallery gallery = category.RibbonGallery;
                if (gallery != null)
                {
                    if (gallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope)
                    {
                        gallery.ArrangeWidth = value;
                        gallery.IsArrangeWidthValid = true;
                    }
                }
            }
        }

        #endregion private methods

        #region Protected overrides

        protected override Size MeasureOverride(Size availableSize)
        {
            // Iterate through all of the children. For each row first measure # of children
            // to infinity and gather their Max of DesiredWidths. Also, calculate the 
            // columnCount. Then space permitting measure as many more 
            // children that will fit into that row till columnCount is acheived and get 
            // MaxHeight for that Row. Cumulative height of such Rows gives you DesiredHeight
            // for Panel.
            // Return desired size for this Panel as the 
            // new Size(ColumnCount * MaxColumnWidth, cumulative RowHeights).

            UIElementCollection children = InternalChildren;
            Size panelSize = new Size();
            Size childConstraint = new Size(double.PositiveInfinity, double.PositiveInfinity);
            double maxRowHeight = 0;
            double maxItemHeight = 0;
            double sumItemHeight = 0;
            int columnCount = 0;
            int childrenCount = children.Count;
            RibbonGallery parentGallery = RibbonGallery;
            double maxColumnWidth = (parentGallery != null && parentGallery.IsMaxColumnWidthValid) ? GetMaxColumnWidth() : 0.0;
            int minColumnCount = GetMinColumnCount();
            int maxColumnCount = GetMaxColumnCount();

            Debug.Assert(maxColumnCount >= minColumnCount);
            
            // Determine the maximum column width so that all items 
            // can be hosted in equispaced columns. row height is auto
            // and depends on the maximum height of the items in that row
            for (int i = 0; i < childrenCount; i++)
            {
                UIElement child = children[i] as UIElement;
                child.Measure(childConstraint);
                Size childSize = child.DesiredSize;
                maxColumnWidth = Math.Max(maxColumnWidth, childSize.Width);
                maxItemHeight = Math.Max(maxItemHeight, childSize.Height);
                sumItemHeight += childSize.Height;
            }

            // if none of the children has substantial width, panelsize would be equivalent to zero.
            if (maxColumnWidth == 0.0)
            {
                return panelSize;
            }

            // Updates the MaxColumnWidth of this Category as well as the of the parent Gallery if suffices all conditions.
            SetMaxColumnWidth(maxColumnWidth);

            // Gets the final MaxColumnWidth for this panel.
            maxColumnWidth = GetMaxColumnWidth();

            if (!IsAutoLayoutPass(sumItemHeight, childrenCount))
            {
                if (!double.IsInfinity(availableSize.Width))
                {
                    columnCount = Math.Min(Math.Max(minColumnCount, Math.Min((int)(availableSize.Width / maxColumnWidth), childrenCount)), maxColumnCount);
                    
                    RibbonGalleryCategory category = Category;
                    if (parentGallery != null && category != null)
                    {
                        if (parentGallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope && parentGallery.ColumnsStretchToFill)
                        {
                            // Since Gallery is a SharedColumnScope, store ArrangeWidth to be shared by the entire Gallery
                            double arrangeWidth = GetArrangeWidth();
                            if (!parentGallery.IsArrangeWidthValid)
                            {
                                // Calculate ArrangeWidth such that columnCount no. of columns occupy all of availableSize.Width
                                columnCount = Math.Min(Math.Max(minColumnCount, Math.Min((int)(availableSize.Width / maxColumnWidth), childrenCount)), maxColumnCount);
                                arrangeWidth = Math.Max(availableSize.Width / columnCount, maxColumnWidth);
                                SetArrangeWidth(arrangeWidth);
                            }
                            else
                            {
                                // Once a valid arrangeWidth has been computed, we use arrangeWidth to determine number of columns. 
                                columnCount = Math.Min(Math.Max(minColumnCount, Math.Min((int)(availableSize.Width / arrangeWidth), childrenCount)), maxColumnCount);
                            }
                        }
                        else if (category.IsSharedColumnSizeScope && category.ColumnsStretchToFill)
                        {
                            // Since category is a sharedColumnScope. 
                            // Calculate and store _arrangeWidth locally for just this category
                            if (!_isArrangeWidthValid)
                            {
                                columnCount = Math.Min(Math.Max(minColumnCount, Math.Min((int)(availableSize.Width / maxColumnWidth), childrenCount)), maxColumnCount);
                                _arrangeWidth = Math.Max(availableSize.Width / columnCount, maxColumnWidth);
                                _isArrangeWidthValid = true;
                            }
                            else
                            {
                                // Once a valid arrangeWidth has been computed, we use arrangeWidth to determine number of columns. 
                                columnCount = Math.Min(Math.Max(minColumnCount, Math.Min((int)(availableSize.Width / _arrangeWidth), childrenCount)), maxColumnCount);
                            }
                        }
                    }
                }
                else
                {
                    columnCount = Math.Max(minColumnCount, Math.Min(childrenCount, maxColumnCount));
                }

                // Finds row Items once ColumnWidth is determined to fetch MaxHeight of a particular row.
                // Also adds these height to acheive cumulative height which is desired height of the panel.
                for (int i = 0; i < childrenCount; i++)
                {
                    UIElement child = children[i] as UIElement;
                    Size childSize = child.DesiredSize;
                    maxRowHeight = Math.Max(maxRowHeight, childSize.Height);
                    if ((i + 1) % columnCount == 0 || i == childrenCount - 1)
                    {
                        panelSize.Height += maxRowHeight;

                        // Save the maxRowHeight so it can be used for PageDown operations
                        _maxRowHeight = maxRowHeight;

                        maxRowHeight = 0;
                    }
                }
            }
            else
            {
                columnCount = minColumnCount;
                panelSize.Height = maxItemHeight;
            }

            panelSize.Width = columnCount * maxColumnWidth;
            return panelSize;
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            // Get final coumn count by finalsizw.width , MaxColumnWidth
            // Iterate through children one row at a time. 
            // Arrange the first row at offset 0 and the next 
            // row just below that and so on. Besure to arrange 
            // the children within each row uniformly based on 
            // the MaxColumnWidth that was computed during the 
            // Measure pass.

            UIElementCollection children = InternalChildren;
            double rowStartHeight = 0;
            double rowStartWidth = 0; 
            double maxRowHeight = 0.0;
            int finalColumnCount = 0;
            int rowStartIndex = 0;
            int childrenCount = children.Count;
            int minColumnCount = GetMinColumnCount();
            int maxColumnCount = GetMaxColumnCount();
            RibbonGallery parentGallery = RibbonGallery;
            RibbonGalleryCategory category = Category;
            double arrangeWidth = 0.0;

            if (parentGallery != null && category != null)
            {
                if (parentGallery.IsSharedColumnSizeScope && !category.IsSharedColumnSizeScope && parentGallery.ColumnsStretchToFill)
                {
                    // If sharedScope is Gallery, fetch global ArrangeWidth
                    arrangeWidth = GetArrangeWidth();
                }
                else if (category.IsSharedColumnSizeScope && category.ColumnsStretchToFill)
                {
                    // SharedScope is Category, use local arrangeWidth.
                    arrangeWidth = _arrangeWidth;
                }
                else
                {
                    // ColumnStretchToFill is false. 
                    arrangeWidth = GetMaxColumnWidth();
                }
            }

            //Calculate the available column count based on final space
            //keeping the same column width. 
            if (arrangeWidth == 0.0)
                return finalSize;

            finalColumnCount = Math.Max(minColumnCount, Math.Min((int)(finalSize.Width / arrangeWidth), maxColumnCount));

            for (int i = 0; i < childrenCount; i++)
            {
                maxRowHeight = Math.Max(maxRowHeight, children[i].DesiredSize.Height);
                if ((i + 1) % finalColumnCount == 0 || i == childrenCount-1)
                {
                    //Arrange the row
                    for (int j = rowStartIndex; j <= i; j++)
                    {
                        children[j].Arrange(new Rect(rowStartWidth, rowStartHeight, arrangeWidth, maxRowHeight));
                        rowStartWidth += arrangeWidth;
                    }
                    rowStartHeight += maxRowHeight;
                    maxRowHeight = 0;
                    rowStartIndex = i + 1;
                    rowStartWidth = 0;
                }
            }

            return finalSize;
        }

        #endregion Protected overrides

        #region data

        private RibbonGallery RibbonGallery
        {
            get
            {
                RibbonGalleryCategory category = (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
                if (category != null)
                {
                    return category.RibbonGallery;
                }
                return null;
            }
        }

        private RibbonGalleryCategory Category
        {
            get
            {
                return (RibbonGalleryCategory)ItemsControl.GetItemsOwner(this);
            }
        }

        internal double MaxColumnWidth
        {
            get { return _maxColumnWidth; }
            private set
            {
                if (_maxColumnWidth != value)
                {
                    _maxColumnWidth = value;
                    _isArrangeWidthValid = false;
                }
            }
        }

        internal double MaxRowHeight
        {
            get { return _maxRowHeight; }
        }


        // this is local value of maxColumnWidth per category
        private double _maxColumnWidth = 0, _arrangeWidth = 0.0;
        private double _maxRowHeight = 0;
        bool _isArrangeWidthValid = false;
        #endregion
    }
}

