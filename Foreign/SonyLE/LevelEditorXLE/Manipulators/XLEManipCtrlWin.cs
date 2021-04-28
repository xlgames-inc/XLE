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

#pragma warning disable 0649 // Field '...' is never assigned to, and will always have its default value null

namespace LevelEditorXLE.Manipulators
{
    public class XLEManipCtrls : UserControl
    { 
        public XLEManipCtrls()
        {
            Margin = new Padding(0);
            Padding = new Padding(0);
            m_tabControl = new TabControl();
            m_tabControl.Dock = DockStyle.Fill;
            m_tabControl.Margin = new Padding(0);
            m_tabControl.Padding = new System.Drawing.Point(0, 0);
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
            foreach (var m in manipulators)
            {
#if GUILAYER_SCENEENGINE
                var t = m as Terrain.TerrainManipulator;
                if (t != null)
                {
                    var terrainCtrls = new XLENativeManipControls();
                    terrainCtrls.Dock = DockStyle.Fill;
                    terrainCtrls.SetActiveContext(t.ManipulatorContext);

                    var terrainPage = new TabPage("Terrain");
                    terrainPage.Tag = m;
                    terrainPage.Controls.Add(terrainCtrls);
                    m_controls.m_tabControl.TabPages.Add(terrainPage);
                }

                var s = m as Placements.ScatterPlaceManipulator;
                if (s != null)
                {
                    var properties = new XLEScatterPlaceControls();
                    properties.Dock = DockStyle.Fill;
                    properties.Object = s.ManipulatorContext;

                    properties.SaveEvent = delegate(object o, string fn)
                        {
                            try
                            {
                                var settings = o as Placements.ScatterPlaceManipulator.ManipulatorSettings;
                                if (settings != null)
                                    settings.SaveModelList(fn);
                            }
                            catch (Exception e) { MessageBox.Show("Error while saving model list " + e.Message); }
                        };

                    properties.LoadEvent = delegate(object o, string fn)
                        {
                            try
                            {
                                var settings = o as Placements.ScatterPlaceManipulator.ManipulatorSettings;
                                if (settings != null)
                                    settings.LoadModelList(fn);

                                // Hack! Problem when refreshing properties after a change
                                // So we need to change the object to something else, and then
                                // change it back.
                                // Note that this creates a hidden cyclic dependency
                                //      -- because we're keeping a reference on the "properties"
                                //          variable here
                                properties.Object = new Placements.ScatterPlaceManipulator.ManipulatorSettings();
                                properties.Object = settings;
                            }
                            catch (Exception e) { MessageBox.Show("Error while loading model list " + e.Message); }
                        };

                    var page = new TabPage("Scatter Placer");
                    page.Tag = m;
                    page.Controls.Add(properties);
                    m_controls.m_tabControl.TabPages.Add(page);
                }
#endif
            }

#if GUILAYER_SCENEENGINE
            {
                var page = new TabPage("Locked Area");
                var lockedAreaCtrls = new Terrain.TerrainContextControls(m_contextRegistry);
                lockedAreaCtrls.Dock = DockStyle.Fill;
                page.Controls.Add(lockedAreaCtrls);
                m_controls.m_tabControl.TabPages.Add(page);
            }
#endif

            m_controls.PerformLayout();

            m_controlHostService.RegisterControl(
                m_controls,
                new ControlInfo("Manipulator Controls", "Properties for the active manipulator", StandardControlGroup.Right),
                null);
        }

        [Import(AllowDefault = false)] IControlHostService m_controlHostService;
        [Import(AllowDefault = false)] IContextRegistry m_contextRegistry;
        XLEManipCtrls m_controls;
    }
}
