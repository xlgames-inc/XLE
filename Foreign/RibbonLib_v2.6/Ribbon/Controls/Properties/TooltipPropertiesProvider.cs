//*****************************************************************************
//
//  File:       TooltipPropertiesProvider.cs
//
//  Contents:   Definition for tooltip properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for tooltip properties provider interface
    /// </summary>
    public interface ITooltipPropertiesProvider
    {
        /// <summary>
        /// Tooltip title property
        /// </summary>
        string TooltipTitle { get; set; }

        /// <summary>
        /// Tooltip description property
        /// </summary>
        string TooltipDescription { get; set; }
    }

    /// <summary>
    /// Implementation of ITooltipPropertiesProvider
    /// </summary>
    public class TooltipPropertiesProvider : BasePropertiesProvider, ITooltipPropertiesProvider
    {
        /// <summary>
        /// TooltipPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public TooltipPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.TooltipTitle);
            _supportedProperties.Add(RibbonProperties.TooltipDescription);
        }

        private string _tooltipTitle;
        private string _tooltipDescription;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.TooltipTitle)
            {
                if (_tooltipTitle != null)
                {
                    newValue.SetString(_tooltipTitle);
                }
            }
            else if (key == RibbonProperties.TooltipDescription)
            {
                if (_tooltipDescription != null)
                {
                    newValue.SetString(_tooltipDescription);
                }
            }
            
            return HRESULT.S_OK;
        }

        #region ITooltipPropertiesProvider Members

        /// <summary>
        /// Tooltip title property
        /// </summary>
        public string TooltipTitle
        {
            get
            {
                return _tooltipTitle;
            }
            set
            {
                _tooltipTitle = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.TooltipTitle));
                }
            }
        }

        /// <summary>
        /// Tooltip description property
        /// </summary>
        public string TooltipDescription
        {
            get
            {
                return _tooltipDescription;
            }
            set
            {
                _tooltipDescription = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.TooltipDescription));
                }
            }
        }

        #endregion
    }
}
