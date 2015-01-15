//*****************************************************************************
//
//  File:       RibbonFontControl.cs
//
//  Contents:   Helper class that wraps a ribbon font control.
//
//*****************************************************************************

using System.Drawing;
using RibbonLib.Controls.Events;
using RibbonLib.Controls.Properties;
using RibbonLib.Interop;
using System;

namespace RibbonLib.Controls
{
    public class RibbonFontControl : BaseRibbonControl, 
        IFontControlPropertiesProvider,
        IEnabledPropertiesProvider, 
        IKeytipPropertiesProvider,
        IExecuteEventsProvider,
        IPreviewEventsProvider
    {
        private FontControlPropertiesProvider _fontControlPropertiesProvider;
        private EnabledPropertiesProvider _enabledPropertiesProvider;
        private KeytipPropertiesProvider _keytipPropertiesProvider;
        private ExecuteEventsProvider _executeEventsProvider;
        private PreviewEventsProvider _previewEventsProvider;

        public RibbonFontControl(Ribbon ribbon, uint commandId)
            : base(ribbon, commandId)
        {
            AddPropertiesProvider(_fontControlPropertiesProvider = new FontControlPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_enabledPropertiesProvider = new EnabledPropertiesProvider(ribbon, commandId));
            AddPropertiesProvider(_keytipPropertiesProvider = new KeytipPropertiesProvider(ribbon, commandId));

            AddEventsProvider(_executeEventsProvider = new ExecuteEventsProvider(this));
            AddEventsProvider(_previewEventsProvider = new PreviewEventsProvider(this));
        }

        #region IFontControlPropertiesProvider Members

        public string Family
        {
            get
            {
                return _fontControlPropertiesProvider.Family;
            }
            set
            {
                _fontControlPropertiesProvider.Family = value;
            }
        }
        
        public decimal Size
        {
            get
            {
                return _fontControlPropertiesProvider.Size;
            }
            set
            {
                _fontControlPropertiesProvider.Size = value;
            }
        }

        public FontProperties Bold
        {
            get
            {
                return _fontControlPropertiesProvider.Bold;
            }
            set
            {
                _fontControlPropertiesProvider.Bold = value;
            }
        }

        public FontProperties Italic
        {
            get
            {
                return _fontControlPropertiesProvider.Italic;
            }
            set
            {
                _fontControlPropertiesProvider.Italic = value;
            }
        }
        
        public FontUnderline Underline
        {
            get
            {
                return _fontControlPropertiesProvider.Underline;
            }
            set
            {
                _fontControlPropertiesProvider.Underline = value;
            }
        }

        public FontProperties Strikethrough
        {
            get
            {
                return _fontControlPropertiesProvider.Strikethrough;
            }
            set
            {
                _fontControlPropertiesProvider.Strikethrough = value;
            }
        }

        public Color ForegroundColor
        {
            get
            {
                return _fontControlPropertiesProvider.ForegroundColor;
            }
            set
            {
                _fontControlPropertiesProvider.ForegroundColor = value;
            }
        }

        public Color BackgroundColor
        {
            get
            {
                return _fontControlPropertiesProvider.BackgroundColor;
            }
            set
            {
                _fontControlPropertiesProvider.BackgroundColor = value;
            }
        }

        public FontVerticalPosition VerticalPositioning
        {
            get
            {
                return _fontControlPropertiesProvider.VerticalPositioning;
            }
            set
            {
                _fontControlPropertiesProvider.VerticalPositioning = value;
            }
        }

        #endregion
        
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

        #region IExecuteEventsProvider Members

        public event EventHandler<ExecuteEventArgs> ExecuteEvent
        {
            add
            {
                _executeEventsProvider.ExecuteEvent += value;
            }
            remove
            {
                _executeEventsProvider.ExecuteEvent -= value;
            }
        }

        #endregion

        #region IPreviewEventsProvider Members

        public event EventHandler<ExecuteEventArgs> PreviewEvent
        {
            add
            {
                _previewEventsProvider.PreviewEvent += value;
            }
            remove
            {
                _previewEventsProvider.PreviewEvent -= value;
            }
        }

        public event EventHandler<ExecuteEventArgs> CancelPreviewEvent
        {
            add
            {
                _previewEventsProvider.CancelPreviewEvent += value;
            }
            remove
            {
                _previewEventsProvider.CancelPreviewEvent -= value;
            }
        }

        #endregion

    }
}
