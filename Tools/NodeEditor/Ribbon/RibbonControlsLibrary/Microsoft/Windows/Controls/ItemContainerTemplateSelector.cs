//---------------------------------------------------------------------------
// <copyright file="ItemContainerTemplateSelector.cs" company="Microsoft Corporation">
//     Copyright (C) Microsoft Corporation.  All rights reserved.
// </copyright>
//---------------------------------------------------------------------------

using System.Windows.Controls;
using System.Windows;
using System;
using System.Collections;
using System.Diagnostics;
using System.Windows.Media;

namespace Microsoft.Windows.Controls
{
    /// <summary>
    ///   A class used to select an ItemContainerTemplate for each item within an ItemsControl
    /// </summary>
    public abstract class ItemContainerTemplateSelector
    {
        /// <summary>
        /// Override this method to return an app specific ItemContainerTemplate
        /// </summary>
        /// <param name="item"></param>
        /// <returns></returns>
        public virtual DataTemplate SelectTemplate(object item, ItemsControl parentItemsControl)
        {
            return null;
        }
    }

    internal class DefaultItemContainerTemplateSelector : ItemContainerTemplateSelector
    {
        public override DataTemplate SelectTemplate(object item, ItemsControl parentItemsControl)
        {
            // Do an implicit type lookup for an ItemContainerTemplate
            return FindTemplateResourceInternal(parentItemsControl, item) as DataTemplate; 
        }

        // Searches through resource dictionaries to find a ItemContainerTemplate
        //  that matches the type of the 'item' parameter.  Failing an exact
        //  match of the type, return something that matches one of its parent
        //  types.
        private static object FindTemplateResourceInternal(DependencyObject target, object item)
        {
            // Data styling doesn't apply to UIElement (bug 1007133).
            if (item == null || (item is UIElement))
            {
                return null;
            }

            Type type = item.GetType();

            ArrayList keys = new ArrayList();

            // construct the list of acceptable keys, in priority order
            int exactMatch = -1;    // number of entries that count as an exact match

            // add compound keys for the dataType and all its base types
            object dataType = type;
            while (dataType != null)
            {
                object key = new ItemContainerTemplateKey(dataType);

                if (key != null)
                    keys.Add(key);

                // all keys added for the given item type itself count as an exact match
                if (exactMatch == -1)
                    exactMatch = keys.Count;

                if (type != null)
                {
                    type = type.BaseType;
                    if (type == typeof(Object))     // don't search for Object - perf
                        type = null;
                }

                dataType = type;
            }

            int bestMatch = keys.Count; // index of best match so far

            // Search the parent chain
            object resource = FindTemplateResourceInTree(target, keys, exactMatch, ref bestMatch);

            if (bestMatch >= exactMatch)
            {
                // Exact match not found in the parent chain.  Try App and System Resources.
                object appResource = FindTemplateResourceFromApp(target, keys, exactMatch, ref bestMatch);

                if (appResource != null)
                    resource = appResource;
            }

            return resource;
        }

        // Search the parent chain for a [Data|Table]Template in a ResourceDictionary.
        private static object FindTemplateResourceInTree(DependencyObject target, ArrayList keys, int exactMatch, ref int bestMatch)
        {
            Debug.Assert(target != null, "Don't call FindTemplateResource with a null target object");

            ResourceDictionary table;
            object resource = null;

            DependencyObject element = target;

            FrameworkElement fe;
            FrameworkContentElement fce;

            while (element != null)
            {
                object candidate;

                fe = element as FrameworkElement;
                fce = (fe == null) ? element as FrameworkContentElement : null;

                // -------------------------------------------
                //  Lookup ResourceDictionary on the current instance
                // -------------------------------------------

                // Fetch the ResourceDictionary
                // for the given target element
                table = GetInstanceResourceDictionary(fe, fce);
                if (table != null)
                {
                    candidate = FindBestMatchInResourceDictionary(table, keys, exactMatch, ref bestMatch);
                    if (candidate != null)
                    {
                        resource = candidate;
                        if (bestMatch < exactMatch)
                        {
                            // Exact match found, stop here.
                            return resource;
                        }
                    }
                }

                // -------------------------------------------
                //  Lookup ResourceDictionary on the current instance's Style, if one exists.
                // -------------------------------------------

                table = GetStyleResourceDictionary(fe, fce);
                if (table != null)
                {
                    candidate = FindBestMatchInResourceDictionary(table, keys, exactMatch, ref bestMatch);
                    if (candidate != null)
                    {
                        resource = candidate;
                        if (bestMatch < exactMatch)
                        {
                            // Exact match found, stop here.
                            return resource;
                        }
                    }
                }

                // -------------------------------------------
                //  We are unable to lookup ResourceDictionary on the current instance's Theme Style in the OOB release.
                // -------------------------------------------

                // -------------------------------------------
                //  Lookup ResourceDictionary on the current instance's Template, if one exists.
                // -------------------------------------------

                table = GetTemplateResourceDictionary(fe, fce);
                if (table != null)
                {
                    candidate = FindBestMatchInResourceDictionary(table, keys, exactMatch, ref bestMatch);
                    if (candidate != null)
                    {
                        resource = candidate;
                        if (bestMatch < exactMatch)
                        {
                            // Exact match found, stop here.
                            return resource;
                        }
                    }
                }

                // We are unable to check inheritance behavior because that property is protected

                // -------------------------------------------
                //  Find the next parent instance to lookup
                // -------------------------------------------

                // Get Framework Parent
                DependencyObject logicalParent = LogicalTreeHelper.GetParent(element);
                element = (logicalParent != null) ? logicalParent : VisualTreeHelper.GetParent(element);

                // We are unable to check inheritance behavior because that property is protected
            }

            return resource;
        }

        // Given a ResourceDictionary and a set of keys, try to find the best
        //  match in the resource dictionary.
        private static object FindBestMatchInResourceDictionary(
            ResourceDictionary table, ArrayList keys, int exactMatch, ref int bestMatch)
        {
            object resource = null;
            int k;

            // Search target element's ResourceDictionary for the resource
            if (table != null)
            {
                for (k = 0; k < bestMatch; ++k)
                {
                    object candidate = table[keys[k]];
                    if (candidate != null)
                    {
                        resource = candidate;
                        bestMatch = k;

                        // if we found an exact match, no need to continue
                        if (bestMatch < exactMatch)
                            return resource;
                    }
                }
            }

            return resource;
        }

        // Return a reference to the ResourceDictionary set on the instance of
        //  the given Framework(Content)Element, if such a ResourceDictionary exists.
        private static ResourceDictionary GetInstanceResourceDictionary(FrameworkElement fe, FrameworkContentElement fce)
        {
            ResourceDictionary table = null;

            if (fe != null)
            {
                table = fe.Resources;
            }
            else if (fce != null)
            {
                table = fce.Resources;
            }

            return table;
        }

        // Return a reference to the ResourceDictionary attached to the Style of
        //  the given Framework(Content)Element, if such a ResourceDictionary exists.
        private static ResourceDictionary GetStyleResourceDictionary(FrameworkElement fe, FrameworkContentElement fce)
        {
            ResourceDictionary table = null;

            if (fe != null)
            {
                if (fe.Style != null)
                {
                    table = fe.Style.Resources;
                }
            }
            else if (fce != null)
            {
                if (fce.Style != null)
                {
                    table = fce.Style.Resources;
                }
            }

            return table;
        }

        // Return a reference to the ResourceDictionary attached to the Template of
        //  the given Framework(Content)Element, if such a ResourceDictionary exists.
        private static ResourceDictionary GetTemplateResourceDictionary(FrameworkElement fe, FrameworkContentElement fce)
        {
            ResourceDictionary table = null;

            if (fe != null)
            {
                Control control = fe as Control;
                if (control != null && control.Template != null)
                {
                    table = control.Template.Resources;
                }
            }

            return table;
        }

        // Find an item container template resource
        internal static object FindTemplateResourceFromApp(DependencyObject target, ArrayList keys, int exactMatch, ref int bestMatch)
        {
            object resource = null;
            int k;

            Application app = Application.Current;
            if (app != null)
            {
                // If the element is rooted to a Window and App exists, defer to App.
                for (k = 0; k < bestMatch; ++k)
                {
                    object appResource = Application.Current.Resources[keys[k]];
                    if (appResource != null)
                    {
                        bestMatch = k;
                        resource = appResource;

                        if (bestMatch < exactMatch)
                            return resource;
                    }
                }
            }

            // We are unable to search the SystemResources in the OOB release

            return resource;
        }
    }
}
