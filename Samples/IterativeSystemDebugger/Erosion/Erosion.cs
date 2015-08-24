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
    public partial class Erosion : Form
    {
        public Erosion(GUILayer.ErosionIterativeSystem system)
        {
            _autoTickState = false;
            _system = system;
            InitializeComponent();
            _previewWindow.Underlying.AddSystem(_system._overlay);

            _previewSettings.SelectedObject = _system._settings;

            _schemaLoader = new ErosionSettingsSchemaLoader();
            _systemSettings.Bind(
                _schemaLoader.CreatePropertyContext(_system._getAndSetProperties));
        }

        protected override void Dispose(bool disposing)
        {
            Application.Idle -= OnAppIdle;
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            if (_system != null) { _system.Dispose(); _system = null; }
            if (_previewWindow!=null) { _previewWindow.Dispose(); }
            base.Dispose(disposing);
        }

        private void _tickButton_Click(object sender, EventArgs e)
        {
            _system.Tick();
            _previewWindow.Invalidate();
        }

        private void OnAppIdle(object sender, EventArgs e)
        {
            _system.Tick();
            _previewWindow.Invalidate();
        }

        private void _autoTick_CheckedChanged(object sender, EventArgs e)
        {
            bool newAutoTickState = _autoTick.CheckState == CheckState.Checked;
            if (newAutoTickState != _autoTickState) {
                _autoTickState = newAutoTickState;
                if (_autoTickState)
                {
                    Application.Idle += OnAppIdle;
                }
                else
                {
                    Application.Idle -= OnAppIdle;
                }
            }
        }

        private GUILayer.ErosionIterativeSystem _system;
        private bool _autoTickState;
        private ErosionSettingsSchemaLoader _schemaLoader;
    }

    public class ErosionSettingsSchemaLoader : XLEBridgeUtils.DataDrivenPropertyContextHelper
    {
        public IPropertyEditingContext CreatePropertyContext(GUILayer.IGetAndSetProperties getAndSet)
        {
            var ps = new GUILayer.BasicPropertySource(
                getAndSet,
                GetPropertyDescriptors("gap:ErosionSettings"));
            return new XLEBridgeUtils.PropertyBridge(ps);
        }

        public ErosionSettingsSchemaLoader()
        {
            SchemaResolver = new Sce.Atf.ResourceStreamResolver(
                System.Reflection.Assembly.GetExecutingAssembly(), 
                "IterativeSystemDebugger.Erosion");
            Load("erosion.xsd");
        }
    };
}
