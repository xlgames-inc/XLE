//*****************************************************************************
//
//  File:       RibbonUIApplication.cs
//
//  Contents:   Class which implements IUIApplication interface which allows 
//              the interaction with the Windows Ribbon Framework.
//
//*****************************************************************************

using System.Runtime.InteropServices;
using RibbonLib.Interop;

namespace RibbonLib
{
    [ComVisible(true)]
    [Guid("B13C3248-093D-4366-9832-A936610846FD")]
    internal class RibbonUIApplication : IUIApplication
    {
        private Ribbon _ribbon;
        private Ribbon _ribbonControl;

        /// <summary>
        /// RibbonUIApplication ctor
        /// </summary>
        /// <param name="ribbon">Ribbon</param>
        /// <param name="form">Form where ribbon should reside</param>
        public RibbonUIApplication(Ribbon ribbon, Ribbon ribbonControl)
        {
            _ribbon = ribbon;
            _ribbonControl = ribbonControl;
        }

        /// <summary>
        /// Reference to IUIRibbon object
        /// </summary>
        public IUIRibbon UIRibbon { get; private set; }


        #region IUIApplication Members

        /// <summary>
        /// Called when the state of a View changes
        /// </summary>
        /// <param name="viewId">The ID for the View. Only a value of 0 is valid.</param>
        /// <param name="typeID">The UI_VIEWTYPE hosted by the application.</param>
        /// <param name="view">A pointer to the View interface.</param>
        /// <param name="verb">The UI_VIEWVERB (or action) performed by the View.</param>
        /// <param name="uReasonCode">Not defined.</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise.</returns>
        public HRESULT OnViewChanged(uint viewId, ViewType typeID, object view, ViewVerb verb, int uReasonCode)
        {
            HRESULT hr = HRESULT.E_FAIL;

            // Checks to see if the view that was changed was a Ribbon view.
            if (typeID == ViewType.Ribbon)
            {
                switch (verb)
                {
                    // The view was newly created
                    case ViewVerb.Create:
                        if (UIRibbon == null)
                        {
                            UIRibbon = view as IUIRibbon;
                        }
                        hr = HRESULT.S_OK;
                        break;

                    // The view has been resized.  For the Ribbon view, the application should
                    // call GetHeight to determine the height of the ribbon.
                    case ViewVerb.Size:
                        uint uRibbonHeight;
                        // Call to the framework to determine the desired height of the Ribbon.
                        hr = UIRibbon.GetHeight(out uRibbonHeight);

                        if (NativeMethods.Failed(hr))
                        {
                            // error
                        }
                        else
                        {
                            _ribbonControl.Height = (int)uRibbonHeight;
                        }
                        break;

                    // The view was destroyed.
                    case ViewVerb.Destroy:

                        UIRibbon = null;
                        hr = HRESULT.S_OK;
                        break;

                    default:
                        break;
                }
            }

            return hr;
        }

        /// <summary>
        /// Called for each Command specified in the Windows Ribbon (Ribbon) framework markup to bind 
        /// the Command to an IUICommandHandler. 
        /// </summary>
        /// <param name="commandId">The ID for the Command, which is specified in the markup resource file.</param>
        /// <param name="typeID">The Command type that is associated with a specific control.</param>
        /// <param name="commandHandler">When this method returns, contains the address of a pointer to an
        /// IUICommandHandler object. This object is a host application Command handler that is bound to one or 
        /// more Commands.</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise.</returns>
        public HRESULT OnCreateUICommand(uint commandId, CommandType typeID, out IUICommandHandler commandHandler)
        {
            commandHandler = _ribbon;
            return HRESULT.S_OK;
        }

        /// <summary>
        /// Called for each Command specified in the Windows Ribbon (Ribbon) framework markup when the 
        /// application window is destroyed. 
        /// </summary>
        /// <param name="commandId">The ID for the Command, which is specified in the markup resource file.</param>
        /// <param name="typeID">The Command type that is associated with a specific control.</param>
        /// <param name="commandHandler">A pointer to an IUICommandHandler object. This value can be NULL.</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise.</returns>
        public HRESULT OnDestroyUICommand(uint commandId, CommandType typeID, IUICommandHandler commandHandler)
        {
            return HRESULT.S_OK;
        }

        #endregion
    }
}
