//*****************************************************************************
//
//  File:       SpinnerPropertiesProvider.cs
//
//  Contents:   Definition for spinner properties provider 
//
//*****************************************************************************

using RibbonLib.Interop;

namespace RibbonLib.Controls.Properties
{
    /// <summary>
    /// Definition for spinner properties provider interface
    /// </summary>
    public interface ISpinnerPropertiesProvider
    {
        /// <summary>
        /// Decimal value property
        /// </summary>
        decimal DecimalValue { get; set; }

        /// <summary>
        /// Increment property
        /// </summary>
        decimal Increment { get; set; }

        /// <summary>
        /// Max value property
        /// </summary>
        decimal MaxValue { get; set; }

        /// <summary>
        /// Min value property
        /// </summary>
        decimal MinValue { get; set; }

        /// <summary>
        /// Decimal places property
        /// </summary>
        uint DecimalPlaces { get; set; }

        /// <summary>
        /// Format string property
        /// </summary>
        string FormatString { get; set; }
    }

    /// <summary>
    /// Implementation of ISpinnerPropertiesProvider
    /// </summary>
    public class SpinnerPropertiesProvider : BasePropertiesProvider, ISpinnerPropertiesProvider
    {
        /// <summary>
        /// SpinnerPropertiesProvider ctor
        /// </summary>
        /// <param name="ribbon">parent ribbon</param>
        /// <param name="commandId">ribbon control command id</param>
        public SpinnerPropertiesProvider(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            // add supported properties
            _supportedProperties.Add(RibbonProperties.DecimalValue);
            _supportedProperties.Add(RibbonProperties.Increment);
            _supportedProperties.Add(RibbonProperties.MaxValue);
            _supportedProperties.Add(RibbonProperties.MinValue);
            _supportedProperties.Add(RibbonProperties.DecimalPlaces);
            _supportedProperties.Add(RibbonProperties.FormatString);
        }

        private decimal? _increment;
        private decimal? _maxValue;
        private decimal? _minValue;
        private uint? _decimalPlaces;
        private string _formatString;

        /// <summary>
        /// Handles IUICommandHandler.UpdateProperty function for the supported properties
        /// </summary>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        public override HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.DecimalValue)
            {
            }
            else if (key == RibbonProperties.Increment)
            {
                if (_increment.HasValue)
                {
                    newValue.SetDecimal(_increment.Value);
                }
            }
            else if (key == RibbonProperties.MaxValue)
            {
                if (_maxValue.HasValue)
                {
                    newValue.SetDecimal(_maxValue.Value);
                }
            }
            else if (key == RibbonProperties.MinValue)
            {
                if (_minValue.HasValue)
                {
                    newValue.SetDecimal(_minValue.Value);
                }
            }
            else if (key == RibbonProperties.DecimalPlaces)
            {
                if (_decimalPlaces.HasValue)
                {
                    newValue.SetUInt(_decimalPlaces.Value);
                }
            }
            else if (key == RibbonProperties.FormatString)
            {
                if (_formatString != null)
                {
                    newValue.SetString(_formatString);
                }
            }

            return HRESULT.S_OK;
        }

        #region ISpinnerPropertiesProvider Members

        /// <summary>
        /// Decimal value property
        /// </summary>
        public decimal DecimalValue
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant decimalValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.DecimalValue, out decimalValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (decimal)decimalValue.Value;
                    }
                }

                return 0;
            }
            set
            {
                if (_ribbon.Initalized)
                {
                    PropVariant decimalValue = PropVariant.FromObject(value);
                    HRESULT hr = _ribbon.Framework.SetUICommandProperty(_commandID, ref RibbonProperties.DecimalValue, ref decimalValue);
                }
            }
        }

        /// <summary>
        /// Increment property
        /// </summary>
        public decimal Increment
        {
            get
            {
                return _increment.GetValueOrDefault();
            }
            set
            {
                _increment = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.Increment));
                }
            }
        }

        /// <summary>
        /// Max value property
        /// </summary>
        public decimal MaxValue
        {
            get
            {
                return _maxValue.GetValueOrDefault();
            }
            set
            {
                _maxValue = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.MaxValue));
                }
            }
        }

        /// <summary>
        /// Min value property
        /// </summary>
        public decimal MinValue
        {
            get
            {
                return _minValue.GetValueOrDefault();
            }
            set
            {
                _minValue = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.MinValue));
                }
            }
        }

        /// <summary>
        /// Decimal places property
        /// </summary>
        public uint DecimalPlaces
        {
            get
            {
                return _decimalPlaces.GetValueOrDefault();
            }
            set
            {
                _decimalPlaces = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.DecimalPlaces));
                }
            }
        }

        /// <summary>
        /// Format string property
        /// </summary>
        public string FormatString
        {
            get
            {
                return _formatString;
            }
            set
            {
                _formatString = value;
                if (_ribbon.Initalized)
                {
                    HRESULT hr = _ribbon.Framework.InvalidateUICommand(_commandID, Invalidations.Property, PropertyKeyRef.From(RibbonProperties.FormatString));
                }
            }
        }

        #endregion
    }
}
