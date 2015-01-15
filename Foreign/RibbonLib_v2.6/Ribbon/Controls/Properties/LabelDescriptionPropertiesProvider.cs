//*****************************************************************************
//
//  File:       LabelDescriptionPropertiesProvider.cs
//
//  Contents:   Definition for label description properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for label description properties provider interface
    /// </summary>
    public interface ILabelDescriptionPropertiesProvider
    {
        /// <summary>
        /// LabelDescription property
        /// </summary>
        string LabelDescription { get; set; }
    }

    /// <summary>
    /// Implementation of ILabelDescriptionPropertiesProvider
    /// </summary>
    public class LabelDescriptionPropertiesProvider : BasePropertiesProvider, ILabelDescriptionPropertiesProvider
    {
        /// <summary>
        /// LabelDescriptionPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public LabelDescriptionPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.LabelDescription);
        }

        private string _labelDescription;
        
        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.LabelDescription)
            {
                if (_labelDescription != null)
                {
                    newValue.SetString(_labelDescription);
                }
            } 
            
            return HRESULT.S_OK;
        }

        #region ILabelDescriptionPropertiesProvider Members

        /// <summary>
        /// Label description property
        /// </summary>
        public string LabelDescription
        {
            get
            {
                return _labelDescription;
            }
            set
            {
                _labelDescription = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.LabelDescription));
                }
            }
        }

        #endregion
    }
}
