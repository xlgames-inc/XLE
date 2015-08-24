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
    public partial class Erosion : BaseWindow
    {
        public Erosion(GUILayer.ErosionIterativeSystem system)
        {
            _system = system;
            _previewWindow.Underlying.AddSystem(_system._overlay);
            _previewSettings.SelectedObject = _system._settings;

            _schemaLoader = new ErosionSettingsSchemaLoader();
            _systemSettings.Bind(
                _schemaLoader.CreatePropertyContext(_system._getAndSetProperties));
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

        private GUILayer.ErosionIterativeSystem _system;
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
