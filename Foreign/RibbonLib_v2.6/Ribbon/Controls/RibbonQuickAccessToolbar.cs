//*****************************************************************************
//
//  File:       RibbonQuickAccessToolbar.cs
//
//  Contents:   Helper class that wraps the ribbon quick access toolbar.
//
//*****************************************************************************

using RibbonLib.Controls.Events;
using RibbonLib.Interop;
using System;

namespace RibbonLib.Controls
{
    public class RibbonQuickAccessToolbar : IRibbonControl, 
        IExecuteEventsProvider
    {
        /// <summary>
        /// reference for parent ribbon class
        /// </summary>
        protected Ribbon _ribbon;

        /// <summary>
        /// QAT command id
        /// </summary>
        protected uint _commandID;

        /// <summary>
        /// handler for the customize button
        /// </summary>
        protected RibbonButton _customizeButton;

        private IUICollection _itemsSource = new UICollection();

        public RibbonQuickAccessToolbar(Ribbon ribbon, uint commandId)
        {
            _ribbon = ribbon;
            _commandID = commandId;
        }

        public RibbonQuickAccessToolbar(Ribbon ribbon, uint commandId, uint customizeCommandId) : this(ribbon, commandId)
        {
            _customizeButton = new RibbonButton(_ribbon, customizeCommandId);
        }

        #region IRibbonControl Members

        public uint CommandID
        {
            get 
            {
                return _commandID;
            }
        }

        public HRESULT Execute(ExecutionVerb verb, PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties)
        {
            return HRESULT.S_OK;
        }

        public HRESULT UpdateProperty(ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
            if (key == RibbonProperties.ItemsSource)
            {
                if (_itemsSource != null)
                {
                    IUICollection itemsSource = (IUICollection)currentValue.PropVariant.Value;

                    itemsSource.Clear();
                    uint count;
                    _itemsSource.GetCount(out count);
                    for (uint i = 0; i < count; ++i)
                    {
                        object item;
                        _itemsSource.GetItem(i, out item);
                        itemsSource.Add(item);
                    }
                }
            }
            return HRESULT.S_OK;
        }

        /// <summary>
        /// Items source property
        /// </summary>
        public IUICollection ItemsSource
        {
            get
            {
                if (_ribbon.Initalized)
                {
                    PropVariant unknownValue;
                    HRESULT hr = _ribbon.Framework.GetUICommandProperty(_commandID, ref RibbonProperties.ItemsSource, out unknownValue);
                    if (NativeMethods.Succeeded(hr))
                    {
                        return (IUICollection)unknownValue.Value;
                    }
                }

                return _itemsSource;
            }
        }

        #endregion

        #region IExecuteEventsProvider Members

        public event EventHandler<ExecuteEventArgs> ExecuteEvent
        {
            add
            { 
                if (_customizeButton != null)
                {
                    _customizeButton.ExecuteEvent += value;
                }
            }
            remove
            {
                if (_customizeButton != null)
                {
                    _customizeButton.ExecuteEvent -= value;
                }
            }
        }

        #endregion
    }
}
