//*****************************************************************************
//
//  File:       GalleryPropertiesProvider.cs
//
//  Contents:   Definition for gallery properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;
using System;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for gallery properties provider interface
    /// </summary>
    public interface IGalleryPropertiesProvider
    {
        /// <summary>
        /// Categories property
        /// </summary>
        IUICollection Categories { get; }

        /// <summary>
        /// Items source property
        /// </summary>
        IUICollection ItemsSource { get; }

        /// <summary>
        /// Selected item property
        /// </summary>
        uint SelectedItem { get; set; }

        /// <summary>
        /// Called when the Categories property is ready to be initialized
        /// </summary>
        event EventHandler<EventArgs> CategoriesReady;

        /// <summary>
        /// Called when the ItemsSource property is ready to be initialized
        /// </summary>
        event EventHandler<EventArgs> ItemsSourceReady;
    }

    /// <summary>
    /// Implementation of IGalleryPropertiesProvider
    /// </summary>
    public class GalleryPropertiesProvider : BasePropertiesProvider, IGalleryPropertiesProvider
    {
        private object _sender;

        /// <summary>
        /// GalleryPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public GalleryPropertiesProvider(Ribbon ribbon, uint commandId, object sender)
            : base(ribbon, commandId)
        {
            _sender = sender;
            
            // add supported properties
            _supportedProperties.Add(RibbonProperties.Categories);
            _supportedProperties.Add(RibbonProperties.ItemsSource);
            _supportedProperties.Add(RibbonProperties.SelectedItem);
        }

        private uint? _selectedItem;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.Categories)
            {
                if (CategoriesReady != null)
                {
                    CategoriesReady(_sender, EventArgs.Empty);
                }
            }
            else if (key == RibbonProperties.ItemsSource)
            {
                if (ItemsSourceReady != null)
                {
                    ItemsSourceReady(_sender, EventArgs.Empty);
                }
            }
            else if (key == RibbonProperties.SelectedItem)
            {
                if (_selectedItem.HasValue)
                {
                    newValue.SetUInt(_selectedItem.Value);
                }
            }

            return HRESULT.S_OK;
        }

        #region IGalleryPropertiesProvider Members

        /// <summary>
        /// Categories property
        /// </summary>
        public IUICollection Categories
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant unknownValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.Categories, out unknownValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (IUICollection)unknownValue.Value;
                    }
                }

                return null;
            }
        }

        /// <summary>
        /// Items source property
        /// </summary>
        public IUICollection ItemsSource
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant unknownValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.ItemsSource, out unknownValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (IUICollection)unknownValue.Value;
                    }
                }

                return null;
            }
        }

        /// <summary>
        /// Selected item property
        /// </summary>
        public uint SelectedItem
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant uintValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.SelectedItem, out uintValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (uint)uintValue.Value;
                    }
                }

                return _selectedItem.GetValueOrDefault(Constants.UI_Collection_InvalidIndex);
            }
            set
            {
                _selectedItem = value;

                if (_ribbon.Initalized)
                {
                    PropVariant uintValue = PropVariant.FromObject(value);
                    HRESULT hr = _ribbon.Framework.SetUICommandProperty(_commandID, ref RibbonProperties.SelectedItem, ref uintValue);
                }
            }
        }

        /// <summary>
        /// Called when the Categories property is ready to be initialized
        /// </summary>
        public event EventHandler<EventArgs> CategoriesReady;

        /// <summary>
        /// Called when the ItemsSource property is ready to be initialized
        /// </summary>
        public event EventHandler<EventArgs> ItemsSourceReady;
    
        #endregion
    }
}
