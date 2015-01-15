//*****************************************************************************
//
//  File:       LabelPropertiesProvider.cs
//
//  Contents:   Definition for label properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for label properties provider interface
    /// </summary>
    public interface ILabelPropertiesProvider
    {
        /// <summary>
        /// Label property
        /// </summary>
        string Label { get; set; }
    }

    /// <summary>
    /// Implementation of ILabelPropertiesProvider
    /// </summary>
    public class LabelPropertiesProvider : BasePropertiesProvider, ILabelPropertiesProvider
    {
        /// <summary>
        /// LabelPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public LabelPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.Label);
        }

        private string _label;
        
        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.Label)
            {
                if (_label != null)
                {
                    newValue.SetString(_label);
                }
            }

            return HRESULT.S_OK;
        }

        #region ILabelPropertiesProvider Members

        /// <summary>
        /// Label property
        /// </summary>
        public string Label
        {
            get
            {
                return _label;
            }
            set
            {
                _label = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.Label));
                }
            }
        }

        #endregion
    }
}
