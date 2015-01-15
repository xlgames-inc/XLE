//*****************************************************************************
//
//  File:       RibbonApplicationMenu.cs
//
//  Contents:   Helper class that wraps a ribbon application menu control.
//
//*****************************************************************************

using RibbonLib.Controls.Properties;

namespace RibbonLib.Controls
{
    public class RibbonApplicationMenu : BaseRibbonControl, 
        ITooltipPropertiesProvider
    {
        private TooltipPropertiesProvider _tooltipPropertiesProvider;

        public RibbonApplicationMenu(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            AddPropertiesProvider(_tooltipPropertiesProvider = new TooltipPropertiesProvider(ribbon, commandId));
        }
 
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
