//*****************************************************************************
//
//  File:       RepresentativeStringPropertiesProvider.cs
//
//  Contents:   Definition for representative string properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for representative string properties provider interface
    /// </summary>
    public interface IRepresentativeStringPropertiesProvider
    {
        /// <summary>
        /// Representative string property
        /// </summary>
        string RepresentativeString { get; set; }
    }

    /// <summary>
    /// Implementation of IRepresentativeStringPropertiesProvider
    /// </summary>
    public class RepresentativeStringPropertiesProvider : BasePropertiesProvider, IRepresentativeStringPropertiesProvider
    {
        /// <summary>
        /// RepresentativeStringPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public RepresentativeStringPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.RepresentativeString);
        }

        private string _representativeString;
        
        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.RepresentativeString)
            {
                if (_representativeString != null)
                {
                    newValue.SetString(_representativeString);
                }
            }
            
            return HRESULT.S_OK;
        }

        #region IRepresentativeStringPropertiesProvider Members

        /// <summary>
        /// Representative string property
        /// </summary>
        public string RepresentativeString
        {
            get
            {
                return _representativeString;
            }
            set
            {
                _representativeString = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.RepresentativeString));
                }
            }
        }

        #endregion
    }
}
