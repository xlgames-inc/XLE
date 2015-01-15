//*****************************************************************************
//
//  File:       RibbonMenuGroup.cs
//
//  Contents:   Helper class that wraps a ribbon menu group control.
//
//*****************************************************************************

using RibbonLib.Controls.Properties;

namespace RibbonLib.Controls
{
    public class RibbonMenuGroup : BaseRibbonControl, 
        IEnabledPropertiesProvider, 
        IKeytipPropertiesProvider,
        ILabelPropertiesProvider,
        ITooltipPropertiesProvider
    {
        private EnabledPropertiesProvider _enabledPropertiesProvider;
        private KeytipPropertiesProvider _keytipPropertiesProvider;
        private LabelPropertiesProvider _labelPropertiesProvider;
        private TooltipPropertiesProvider _tooltipPropertiesProvider;

        public RibbonMenuGroup(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            AddPropertiesProvider(_enabledPropertiesProvider = new EnabledPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_keytipPropertiesProvider = new KeytipPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_labelPropertiesProvider = new LabelPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_tooltipPropertiesProvider = new TooltipPropertiesProvider(ribbon, commandId));
        }

        #region IEnabledPropertiesProvider Members

        public bool Enabled
        {
            get
            {
                return _enabledPropertiesProvider.Enabled;
            }
            set
            {
                _enabledPropertiesProvider.Enabled = value;
            }
        }

        #endregion

        #region IKeytipPropertiesProvider Members

        public string Keytip
        {
            get
            {
                return _keytipPropertiesProvider.Keytip;
            }
            set
            {
                _keytipPropertiesProvider.Keytip = value;
            }
        }

        #endregion

        #region ILabelPropertiesProvider Members

        public string Label
        {
            get
            {
                return _labelPropertiesProvider.Label;
            }
            set
            {
                _labelPropertiesProvider.Label = value;
            }
        }

        #endregion

        #region ITooltipPropertiesProvider Members

        public string TooltipTitle
        {
            get
            {
                return _tooltipPropertiesProvider.TooltipTitle;
            }
            set
            {
                _tooltipPropertiesProvider.TooltipTitle = value;
            }
        }

        public string TooltipDescription
        {
            get
            {
                return _tooltipPropertiesProvider.TooltipDescription;
            }
            set
            {
                _tooltipPropertiesProvider.TooltipDescription = value;
            }
        }

        #endregion
    }
}
