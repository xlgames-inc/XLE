using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

using Sce.Atf.Applications;

namespace IterativeSystemDebugger
{
    public partial class RefCFD : BaseWindow
    {
        public RefCFD(GUILayer.IterativeSystem system, String settingsName)
        {
            _hasOldMouse = false;
            _system = system;
            _previewWindow.Underlying.AddSystem(_system.Overlay);
            _previewWindow.Underlying.AddSystem(_system.WidgetsOverlay);
            _previewSettings.SelectedObject = _system.PreviewSettings;

            _schemaLoader = new CFDSettingsSchemaLoader(settingsName);
            _systemSettings.Bind(
                _schemaLoader.CreatePropertyContext(_system.SimulationSettings));

            // _previewWindow.MouseDown += _previewWindow_MouseDown;
            _previewWindow.MouseMove += _previewWindow_MouseMove;
            _previewWindow.MouseEnter += _previewWindow_MouseEnter;
        }

        void _previewWindow_MouseEnter(object sender, EventArgs e)
        {
            _hasOldMouse = false;
        }

        private uint AsMouseButtonFlags(MouseButtons buttons)
        {
            var result = 0u;
            if ((buttons & MouseButtons.Left) !=0) result |= 1<<0;
            if ((buttons & MouseButtons.Right) != 0) result |= 1<<1;
            if ((buttons & MouseButtons.Middle) != 0) result |= 1<<2;
            return result;
        }

        void _previewWindow_MouseMove(object sender, MouseEventArgs e)
        {
            float swipeX = 0.0f, swipeY = 0.0f;
            if (_hasOldMouse) {
                swipeX = (float)(e.X - _oldMouse.X);
                swipeY = (float)(e.Y - _oldMouse.Y);
            }
            _oldMouse = new Point(e.X, e.Y);
            _hasOldMouse = true;

            _system.OnMouseMove(
                e.X / (float)_previewWindow.ClientSize.Width,
                e.Y / (float)_previewWindow.ClientSize.Height,
                swipeX, swipeY,
                AsMouseButtonFlags(e.Button));
        }

        protected override void Dispose(bool disposing)
        {
            if (_system != null) { _system.Dispose(); _system = null; }
            base.Dispose(disposing);
        }

        protected override void DoTick()
        {
            _system.Tick();
        }

        private GUILayer.IterativeSystem _system;
        private CFDSettingsSchemaLoader _schemaLoader;
        private Point _oldMouse;
        private bool _hasOldMouse;
    }

    public class CFDSettingsSchemaLoader : XLEBridgeUtils.DataDrivenPropertyContextHelper
    {
        public IPropertyEditingContext CreatePropertyContext(GUILayer.IGetAndSetProperties getAndSet)
        {
            var ps = new GUILayer.BasicPropertySource(
                getAndSet,
                GetPropertyDescriptors("gap:" + _settingsName));
            return new XLEBridgeUtils.PropertyBridge(ps);
        }

        public CFDSettingsSchemaLoader(String settingsName)
        {
            _settingsName = settingsName;
            SchemaResolver = new Sce.Atf.ResourceStreamResolver(
                System.Reflection.Assembly.GetExecutingAssembly(), 
                "IterativeSystemDebugger.CFD");
            Load("cfd.xsd");
        }

        private String _settingsName;
    };
}
