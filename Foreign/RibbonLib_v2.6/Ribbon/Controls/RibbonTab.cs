//*****************************************************************************
//
//  File:       RibbonTab.cs
//
//  Contents:   Helper class that wraps a ribbon tab control.
//
//*****************************************************************************

using RibbonLib.Controls.Properties;

namespace RibbonLib.Controls
{
    public class RibbonTab : BaseRibbonControl,
        IKeytipPropertiesProvider,
        ILabelPropertiesProvider,
        ITooltipPropertiesProvider
    {
        private KeytipPropertiesProvider _keytipPropertiesProvider;
        private LabelPropertiesProvider _labelPropertiesProvider;
        private TooltipPropertiesProvider _tooltipPropertiesProvider;

        public RibbonTab(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            AddPropertiesProvider(_keytipPropertiesProvider = new KeytipPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_labelPropertiesProvider = new LabelPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_tooltipPropertiesProvider = new TooltipPropertiesProvider(ribbon, commandId));
        }

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
