//*****************************************************************************
//
//  File:       Ribbon.cs
//
//  Contents:   Class which is used as a façade for the Windows Ribbon 
//              Framework. In charge of initialization and communication with 
//              the Windows Ribbon Framework.
//
//*****************************************************************************

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using RibbonLib.Interop;
using System.Resources;
using System.Threading;
using System.Globalization;
using System.Windows.Forms;
using System.ComponentModel;

namespace RibbonLib
{
    /// <summary>
    /// Main class for using the windows ribbon in a .NET application
    /// </summary>
    public class Ribbon : Control, IUICommandHandler
    {
        private IUIImageFromBitmap _imageFromBitmap;
        private RibbonUIApplication _application;
        private Dictionary<uint, IRibbonControl> _mapRibbonControls = new Dictionary<uint, IRibbonControl>();
        private IntPtr _loadedDllHandle = IntPtr.Zero;

        private const string DefaultResourceName = "APPLICATION_RIBBON";

        private RibbonShortcutTable _ribbonShortcutTable;

        private string _shortcutTableResourceName;


        public string ShortcutTableResourceName
        {
            get { return _shortcutTableResourceName; }
            set
            {
                _shortcutTableResourceName = value;
                CheckInitialize();
            }
        }

        void TryCreateShortcutTable(Assembly assembly)
        {
            _ribbonShortcutTable = null;

            if (string.IsNullOrEmpty(this.ShortcutTableResourceName))
                return;

            _ribbonShortcutTable = Util.DeserializeEmbeddedResource<RibbonShortcutTable>(
                this.ShortcutTableResourceName, assembly);

            var form = this.FindForm();
            form.KeyPreview = true;
            form.KeyUp += new KeyEventHandler(form_KeyUp);
        }

        void form_KeyUp(object sender, KeyEventArgs e)
        {
            if (_ribbonShortcutTable == null)
                return;

            var commandId = _ribbonShortcutTable.HitTest(e.KeyData);
            if (commandId == 0)
                return;

            this.Execute(commandId, ExecutionVerb.Execute, null, null, null);

            e.SuppressKeyPress = false;
            e.Handled = true;
        }

        public Ribbon()
        {
            base.Dock = DockStyle.Top;

            if (Util.DesignMode)
                return;

            this.SetStyle(ControlStyles.UserPaint, false);
            this.SetStyle(ControlStyles.Opaque, true);

            this.HandleCreated += new EventHandler(RibbonControl_HandleCreated);
            this.HandleDestroyed += new EventHandler(Ribbon_HandleDestroyed);
        }

        void Ribbon_HandleDestroyed(object sender, EventArgs e)
        {
            DestroyFramework();
        }

        void RibbonControl_HandleCreated(object sender, EventArgs e)
        {
            CheckInitialize();
        }

        [DefaultValue(typeof(DockStyle), "Top")]
        public override DockStyle Dock
        {
            get
            {
                return base.Dock;
            }
            set
            {
            }
        }

        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public override string Text
        {
            get
            {
                return base.Text;
            }
            set
            {
                base.Text = value;
            }
        }

        string _resourceName;

        public string ResourceName
        {
            get { return _resourceName; }
            set
            {
                _resourceName = value;
                CheckInitialize();
            }
        }

        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public IntPtr WindowHandle
        {
            get
            {
                var form = this.Parent as Form;
                return form.Handle;
            }
        }

        void CheckInitialize()
        {
            if (Util.DesignMode)
                return;

            if (Initalized)
                return;

            if (string.IsNullOrEmpty(ResourceName))
                return;

            var form = this.Parent as Form;
            if (form == null)
                return;

            if (!form.IsHandleCreated)
                return;

            var assembly = form.GetType().Assembly;
            InitFramework(this.ResourceName, assembly);
            TryCreateShortcutTable(assembly);
        }

        /// <summary>
        /// Draws only in Design mode
        /// </summary>
        /// <param name="e"></param>
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            ControlPaint.DrawContainerGrabHandle(e.Graphics, this.ClientRectangle);
        }
        
        /// <summary>
        /// Check if ribbon framework has been initialized
        /// </summary>
        public bool Initalized
        {
            get
            {
                return (Framework != null);
            }
        }

        /// <summary>
        /// Get ribbon framework object
        /// </summary>
        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public IUIFramework Framework { get; private set; }

        string _tempDllFilename;

        byte[] GetLocalizedRibbon(string ribbonResource, Assembly ribbonAssembly)
        {
            byte[] data = null;
            bool found = false;

            // try to get from current current culture satellite assembly first
            var culture = Thread.CurrentThread.CurrentUICulture;
            Assembly satelliteAssembly = null;
            TryGetSatelliteAssembly(culture, ribbonAssembly, ref satelliteAssembly);

            found = TryGetRibbon(ribbonResource, satelliteAssembly, ref data);
            if (found)
                return data;

            // try to get from current current culture fallback satellite assembly
            Assembly fallbackAssembly = null;
            if(culture.Parent != null)
                TryGetSatelliteAssembly(culture.Parent, ribbonAssembly, ref fallbackAssembly);

            found = TryGetRibbon(ribbonResource, fallbackAssembly, ref data);
            if (found)
                return data;

            // try to get from current current culture fallback satellite assembly
            found = TryGetRibbon(ribbonResource, ribbonAssembly, ref data);
            if (!found)
                throw new ArgumentException(string.Format("Ribbon resource '{0}' not found in assembly '{1}'.", ribbonResource, ribbonAssembly.Location));

            return data;
        }
        
        bool TryGetSatelliteAssembly(CultureInfo culture, Assembly mainAssembly, ref Assembly satelliteAssembly)
        {
            try
            {
                satelliteAssembly = mainAssembly.GetSatelliteAssembly(culture);
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        

        bool TryGetRibbon(string ribbonResource, Assembly assembly, ref byte[] data)
        {
            try
            {
                string path = Path.GetTempPath();
                _tempDllFilename = Path.Combine(path, Path.GetTempFileName());
                var buffer = Util.GetEmbeddedResource(ribbonResource, assembly);
                File.WriteAllBytes(_tempDllFilename, buffer);
                data = buffer;
                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }

        /// <summary>
        /// Initalize ribbon framework
        /// </summary>
        /// <param name="form">Form where ribbon should reside</param>
        void InitFramework(string ribbonResource, Assembly ribbonAssembly)
        {
            string path = Path.Combine(Path.GetTempPath(), "RibbonDlls");
            _tempDllFilename = Path.Combine(path, Path.GetTempFileName());
            var buffer = GetLocalizedRibbon(ribbonResource, ribbonAssembly);

            File.WriteAllBytes(_tempDllFilename, buffer);

            // if ribbon dll exists, use it
            if (File.Exists(_tempDllFilename))
            {
                // load ribbon from ribbon dll resource
                InitFramework(DefaultResourceName, _tempDllFilename);
            }
        }

        /// <summary>
        /// Initalize ribbon framework
        /// </summary>
        /// <param name="form">Form where ribbon should reside</param>
        /// <param name="resourceName">Identifier of the ribbon resource</param>
        /// <param name="ribbonDllName">Dll name where to find ribbon resource</param>
        void InitFramework(string resourceName, string ribbonDllName)
        {
            // dynamically load ribbon library
            _loadedDllHandle = NativeMethods.LoadLibraryEx(ribbonDllName, IntPtr.Zero,
                                                            NativeMethods.DONT_RESOLVE_DLL_REFERENCES |
                                                            NativeMethods.LOAD_IGNORE_CODE_AUTHZ_LEVEL |
                                                            NativeMethods.LOAD_LIBRARY_AS_DATAFILE |
                                                            NativeMethods.LOAD_LIBRARY_AS_IMAGE_RESOURCE);

            if (_loadedDllHandle == IntPtr.Zero)
            {
                throw new ApplicationException("Ribbon resource DLL exists but could not be loaded.");
            }

            InitFramework(resourceName, _loadedDllHandle);
        }

        /// <summary>
        /// Initalize ribbon framework
        /// </summary>
        /// <param name="form">Form where ribbon should reside</param>
        /// <param name="resourceName">Identifier of the ribbon resource</param>
        /// <param name="hInstance">Pointer to HINSTANCE of module where we can find ribbon resource</param>
        void InitFramework(string resourceName, IntPtr hInstance)
        {
            // create ribbon framework object
            Framework = CreateRibbonFramework();
            _imageFromBitmap = CreateImageFromBitmapFactory();

            // create ribbon application object
            _application = new RibbonUIApplication(this, this);

            // init ribbon framework
            HRESULT hr = Framework.Initialize(this.WindowHandle, _application);
            if (NativeMethods.Failed(hr))
            {
                Marshal.ThrowExceptionForHR((int)hr);
            }

            // load ribbon ui
            hr = Framework.LoadUI(hInstance, resourceName);


            if (NativeMethods.Failed(hr))
            {
                Marshal.ThrowExceptionForHR((int)hr);
            }
        }
        
        /// <summary>
        /// Destroy ribbon framework
        /// </summary>
        void DestroyFramework()
        {
           
            if (Initalized)
            {
                // destroy ribbon framework
                Framework.Destroy();
                Marshal.ReleaseComObject(Framework);

                // remove reference to framework object
                Framework = null;
            }

            if (_loadedDllHandle != IntPtr.Zero)
            {
                // free dynamic library
                NativeMethods.FreeLibrary(_loadedDllHandle);
                _loadedDllHandle = IntPtr.Zero;
            }

            if (_imageFromBitmap != null)
            {
                // remove reference to imageFromBitmap object
                _imageFromBitmap = null;
            }

            if (_application != null)
            {
                // remove reference to application object
                _application = null;
            }

            // remove references to ribbon controls
            _mapRibbonControls.Clear();

            if (!string.IsNullOrEmpty(_tempDllFilename))
            {
                try
                {
                    File.Delete(_tempDllFilename);
                    _tempDllFilename = null;
                }
                catch { }
            }
        }

        /// <summary>
        /// Change ribbon background, highlight and text colors
        /// </summary>
        /// <param name="background">new background color</param>
        /// <param name="highlight">new highlight color</param>
        /// <param name="text">new text color</param>
        public void SetColors(Color background, Color highlight, Color text)
        {
            if (!Initalized)
            {
                return;
            }

            // convert colors to proper color format
            uint backgroundColor = ColorHelper.HSB2Uint(ColorHelper.HSL2HSB(ColorHelper.RGB2HSL(background)));
            uint highlightColor = ColorHelper.HSB2Uint(ColorHelper.HSL2HSB(ColorHelper.RGB2HSL(highlight)));
            uint textColor = ColorHelper.HSB2Uint(ColorHelper.HSL2HSB(ColorHelper.RGB2HSL(text)));
            
            IPropertyStore propertyStore = (IPropertyStore)Framework;

            PropVariant backgroundColorProp = PropVariant.FromObject(backgroundColor);
            PropVariant highlightColorProp = PropVariant.FromObject(highlightColor);
            PropVariant textColorProp = PropVariant.FromObject(textColor);

            // set ribbon colors
            propertyStore.SetValue(ref RibbonProperties.GlobalBackgroundColor, ref backgroundColorProp);
            propertyStore.SetValue(ref RibbonProperties.GlobalHighlightColor, ref highlightColorProp);
            propertyStore.SetValue(ref RibbonProperties.GlobalTextColor, ref textColorProp);

            propertyStore.Commit();
        }

        /// <summary>
        /// Wraps a bitmap object with IUIImage interface
        /// </summary>
        /// <param name="bitmap">bitmap object to wrap</param>
        /// <returns>IUIImage wrapper</returns>
        public IUIImage ConvertToUIImage(Bitmap bitmap)
        {
            if (_imageFromBitmap == null)
            {
                return null;
            }

            IUIImage uiImage;
            _imageFromBitmap.CreateImage(bitmap.GetHbitmap(), Ownership.Transfer, out uiImage);

            return uiImage;
        }

        /// <summary>
        /// Set current application modes
        /// </summary>
        /// <param name="modesArray">array of modes to set</param>
        /// <remarks>Unlisted modes will be unset</remarks>
        public void SetModes(params byte[] modesArray)
        {
            // check that ribbon is initialized
            if (!Initalized)
            {
                return;
            }

            // calculate compact modes value
            int compactModes = 0;
            for (int i = 0; i < modesArray.Length; ++i)
            {
                if (modesArray[i] >= 32)
                {
                    throw new ArgumentException("Modes should range between 0 to 31.");
                }

                compactModes |= (1 << modesArray[i]);
            }

            // set modes
            Framework.SetModes(compactModes);        
        }

        /// <summary>
        /// Shows a predefined context popup in a specific location
        /// </summary>
        /// <param name="contextPopupID">commandId for the context popup</param>
        /// <param name="x">X in screen coordinates</param>
        /// <param name="y">Y in screen coordinates</param>
        public void ShowContextPopup(uint contextPopupID, int x, int y)
        {
            // check that ribbon is initialized
            if (!Initalized)
            {
                return;
            }

            object contextualUIObject;
            Guid contextualUIGuid = new Guid(RibbonIIDGuid.IUIContextualUI);
            HRESULT hr = Framework.GetView(contextPopupID, ref contextualUIGuid, out contextualUIObject);
            if (NativeMethods.Succeeded(hr))
            {
                IUIContextualUI contextualUI = contextualUIObject as IUIContextualUI;
                contextualUI.ShowAtLocation(x, y);
                Marshal.ReleaseComObject(contextualUI);
            }
            else
            {
                Marshal.ThrowExceptionForHR((int)hr);
            }
        }

        /// <summary>
        /// Specifies whether the ribbon is in a collapsed or expanded state
        /// </summary>
        public bool Minimized
        {
            get
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return false;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propMinimized;
                HRESULT hr = propertyStore.GetValue(ref RibbonProperties.Minimized, out propMinimized);
                return (bool)propMinimized.Value;
            }
            set
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propMinimized = PropVariant.FromObject(value);
                HRESULT hr = propertyStore.SetValue(ref RibbonProperties.Minimized, ref propMinimized);
                hr = propertyStore.Commit();
            }
        }

        /// <summary>
        /// Specifies whether the ribbon user interface (UI) is in a visible or hidden state
        /// </summary>
        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public bool Viewable
        {
            get
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return false;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propViewable;
                HRESULT hr = propertyStore.GetValue(ref RibbonProperties.Viewable, out propViewable);
                return (bool)propViewable.Value;
            }
            set
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propViewable = PropVariant.FromObject(value);
                HRESULT hr = propertyStore.SetValue(ref RibbonProperties.Viewable, ref propViewable);
                hr = propertyStore.Commit();
            }
        }

        /// <summary>
        /// Specifies whether the quick access toolbar is docked at the top or at the bottom
        /// </summary>
        [Browsable(false), DesignerSerializationVisibility(DesignerSerializationVisibility.Hidden)]
        public ControlDock QuickAccessToolbarDock
        {
            get
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return ControlDock.Top;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propQuickAccessToolbarDock;
                HRESULT hr = propertyStore.GetValue(ref RibbonProperties.QuickAccessToolbarDock, out propQuickAccessToolbarDock);
                return (ControlDock)(uint)propQuickAccessToolbarDock.Value;
            }
            set
            {
                // check that ribbon is initialized
                if (!Initalized)
                {
                    return;
                }

                IPropertyStore propertyStore = _application.UIRibbon as IPropertyStore;
                PropVariant propQuickAccessToolbarDock = PropVariant.FromObject((uint)value);
                HRESULT hr = propertyStore.SetValue(ref RibbonProperties.QuickAccessToolbarDock, ref propQuickAccessToolbarDock);
                hr = propertyStore.Commit();
            }
        }

        public void SaveSettingsToStream(Stream stream)
        {
            if (!Initalized)
            {
                return;
            }

            StreamAdapter streamAdapter = new StreamAdapter(stream);
            HRESULT hr = _application.UIRibbon.SaveSettingsToStream(streamAdapter);
        }

        public void LoadSettingsFromStream(Stream stream)
        {
            if (!Initalized)
            {
                return;
            }

            StreamAdapter streamAdapter = new StreamAdapter(stream);
            HRESULT hr = _application.UIRibbon.LoadSettingsFromStream(streamAdapter);
        }

        /// <summary>
        /// Create ribbon framework object
        /// </summary>
        /// <returns>ribbon framework object</returns>
        private static IUIFramework CreateRibbonFramework()
        {
            try
            {
                return new UIRibbonFramework() as IUIFramework;
            }
            catch (COMException exception)
            {
                throw new PlatformNotSupportedException("The ribbon framework couldn't be found on this system.", exception);
            }
        }

        /// <summary>
        /// Create image-from-bitmap factory object
        /// </summary>
        /// <returns>image-from-bitmap factory object</returns>
        private static IUIImageFromBitmap CreateImageFromBitmapFactory()
        {
            return new UIRibbonImageFromBitmapFactory() as IUIImageFromBitmap;
        }

        /// <summary>
        /// Generates a default ribbon dll name
        /// </summary>
        /// <returns>name of the dll</returns>
        private string GenerateDefaultRibbonDllName()
        {
            return Path.ChangeExtension(new Uri(Assembly.GetEntryAssembly().CodeBase).LocalPath, ".ribbon.dll");
        }
        
        /// <summary>
        /// Adds a ribbon control to the internal map
        /// </summary>
        /// <param name="ribbonControl">ribbon control to add</param>
        internal void AddRibbonControl(IRibbonControl ribbonControl)
        {
            _mapRibbonControls.Add(ribbonControl.CommandID, ribbonControl);
        }
        
        #region Implementation of IUICommandHandler

        /// <summary>
        /// Implementation of IUICommandHandler.Execute
        /// Responds to execute events on Commands bound to the Command handler
        /// </summary>
        /// <param name="commandID">the command that has been executed</param>
        /// <param name="verb">the mode of execution</param>
        /// <param name="key">the property that has changed</param>
        /// <param name="currentValue">the new value of the property that has changed</param>
        /// <param name="commandExecutionProperties">additional data for this execution</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        /// <remarks>This method is used internally by the Ribbon class and should not be called by the user.</remarks>
        public virtual HRESULT Execute(uint commandID, ExecutionVerb verb, PropertyKeyRef key, PropVariantRef currentValue, IUISimplePropertySet commandExecutionProperties)
        {
#if DEBUG
            Debug.WriteLine(string.Format("Execute verb: {0} for command {1}", verb, commandID));
#endif
            
            if (_mapRibbonControls.ContainsKey(commandID))
            {
                return _mapRibbonControls[commandID].Execute(verb, key, currentValue, commandExecutionProperties);
            }

            return HRESULT.S_OK;
        }

        /// <summary>
        /// Implementation of IUICommandHandler.UpdateProperty
        /// Responds to property update requests from the Windows Ribbon (Ribbon) framework. 
        /// </summary>
        /// <param name="commandID">The ID for the Command, which is specified in the Markup resource file</param>
        /// <param name="key">The Property Key to update</param>
        /// <param name="currentValue">A pointer to the current value for key. This parameter can be NULL</param>
        /// <param name="newValue">When this method returns, contains a pointer to the new value for key</param>
        /// <returns>Returns S_OK if successful, or an error value otherwise</returns>
        /// <remarks>This method is used internally by the Ribbon class and should not be called by the user.</remarks>
        public virtual HRESULT UpdateProperty(uint commandID, ref PropertyKey key, PropVariantRef currentValue, ref PropVariant newValue)
        {
#if DEBUG
            Debug.WriteLine(string.Format("UpdateProperty key: {0} for command {1}", RibbonProperties.GetPropertyKeyName(ref key), commandID));
#endif

            if (_mapRibbonControls.ContainsKey(commandID))
            {
                return _mapRibbonControls[commandID].UpdateProperty(ref key, currentValue, ref newValue);
            }

            return HRESULT.S_OK;
        }

        #endregion
    }
}
