// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.ComponentModel.Composition;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditorXLE.Manipulators
{
    public class XLEManipCtrls : UserControl
    { 
        public XLEManipCtrls()
        {
            m_tabControl = new TabControl();
            m_tabControl.Dock = DockStyle.Fill;
            Controls.Add(m_tabControl);
        }

        public TabControl m_tabControl;
    }

    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLEManipCtrlWin : IInitializable
    {
        void IInitializable.Initialize()
        {
            m_controls = new XLEManipCtrls();

                // We could use another MEF container system to initialise the
                // tab pages for this control... But for now, this is a simple
                // way to get the behaviour we want.
            var manipulators = Globals.MEFContainer.GetExportedValues<IManipulator>();
            foreach (var m in manipulators) {
                var t = m as XLELayer.TerrainManipulator;
                if (t!=null) {
                    var terrainCtrls = new XLENativeManipControls();
                    terrainCtrls.SetActiveContext(t.ManipulatorContext);

                    var terrainPage = new TabPage("Terrain");
                    terrainPage.Tag = m;
                    terrainPage.Controls.Add(terrainCtrls);
                    m_controls.m_tabControl.TabPages.Add(terrainPage);
                }

                var s = m as RenderingInterop.ScatterPlaceManipulator;
                if (s != null) {
                    var properties = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
                    properties.Dock = DockStyle.Fill;
                    properties.Bind(s.ManipulatorContext);

                    var page = new TabPage("Scatter Placer");
                    page.Tag = m;
                    page.Controls.Add(properties);
                    m_controls.m_tabControl.TabPages.Add(page);
                }
            }

            m_controlHostService.RegisterControl(
                m_controls,
                new ControlInfo("Manipulator Controls", "Properties for the active manipulator", StandardControlGroup.Right),
                null);
        }

        [Import(AllowDefault = false)] IControlHostService m_controlHostService;
        XLEManipCtrls m_controls;
    }
}
