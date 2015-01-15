//*****************************************************************************
//
//  File:       KeytipPropertiesProvider.cs
//
//  Contents:   Definition for keytip properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for keytip properties provider interface
    /// </summary>
    public interface IKeytipPropertiesProvider
    {
        /// <summary>
        /// Keytip property
        /// </summary>
        string Keytip { get; set; }
    }

    /// <summary>
    /// Implementation of IKeytipPropertiesProvider
    /// </summary>
    public class KeytipPropertiesProvider : BasePropertiesProvider, IKeytipPropertiesProvider
    {
        /// <summary>
        /// KeytipPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public KeytipPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        { 
            // add supported properties
            _supportedProperties.Add(RibbonProperties.Keytip);
        }

        private string _keytip;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.Keytip)
            {
                if (_keytip != null)
                {
                    newValue.SetString(_keytip);
                }
            }
            
            return HRESULT.S_OK;
        }

        #region IKeytipPropertiesProvider Members

        /// <summary>
        /// Keytip property
        /// </summary>
        public string Keytip
        {
            get
            {
                return _keytip;
            }
            set
            {
                _keytip = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.Keytip));
                }
            }
        }

        #endregion
    }
}
