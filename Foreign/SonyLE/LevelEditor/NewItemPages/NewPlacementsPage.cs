using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.Windows.Forms;
using Sce.Atf.Controls;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;

namespace LevelEditor.NewItemPages
{
    [Export(typeof(NewItemPage))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class NewPlacementsPage : NewItemPage
    {
        public Control Control { get { return m_control; } }

        NewPlacementsPage()
        {
            m_config = new Config();
            m_control = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
            m_control.Dock = DockStyle.Fill;
            m_control.Visible = true;

            m_control.Bind((new Helper()).CreatePropertyContext(m_config));
        }

        public void Execute(IAdaptable parent)
        {

        }

        public bool CanOperateOn(IAdaptable parent)
        {
            // return parent.Is<DomNodeAdapters.Game>()
            //     || parent.Is<DomNodeAdapters.PlacementsFolder>();
            return false;
        }

        private class Config
        {
            internal uint CellCountX { get; set; }
            internal uint CellCountY { get; set; }
            internal uint CellSize { get; set; }
            internal Config() { CellCountX = 4; CellCountY = 4; CellSize = 512; }
        };

            // we could use the xml schema setup for attaching property descriptors
            // to the "Config" object -- or we could try using annotations
        private class Helper : XLELayer.DataDrivenPropertyContextHelper
        {
            internal IPropertyEditingContext CreatePropertyContext(Object obj)
            {
                return new XLELayer.BasicPropertyEditingContext(
                    obj, GetPropertyDescriptors("gap:NewPlacementsPage"));
            }

            internal Helper()
            {
                SchemaResolver = new Sce.Atf.ResourceStreamResolver(
                    System.Reflection.Assembly.GetExecutingAssembly(), ".");
                Load("newitempages.xsd");
            }
        };

        private Config m_config;
        private Sce.Atf.Controls.PropertyEditing.PropertyGrid m_control;
    }
}
