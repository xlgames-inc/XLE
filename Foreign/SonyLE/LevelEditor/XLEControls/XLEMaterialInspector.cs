using System;
using System.ComponentModel.Composition;
using System.Collections.Generic;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Dom;

using LevelEditorCore;

using ControlsLibrary.MaterialEditor;

namespace LevelEditor.XLEControls
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class XLEMaterialInspector : IInitializable
    {
        void IInitializable.Initialize()
        {
            // m_controls = new HierchicalMaterialControl();

            m_controls = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
            m_controls.Location = new System.Drawing.Point(158, 3);
            m_controls.Name = "manipulatorProperties";
            m_controls.Size = new System.Drawing.Size(341, 188);
            m_controls.TabIndex = 1;

            Context.OnChange += OnActiveMaterialChange;

            m_controlHostService.RegisterControl(
                m_controls,
                new ControlInfo("Material inspector", "Properties for the picked material", StandardControlGroup.Right),
                null);
        }

        void OnActiveMaterialChange()
        {
            var env = new GUILayer.EnvironmentSettingsSet(RenderingInterop.GameEngine.GetEditorSceneManager());
            env.AddDefault();
            // m_controls.EnvironmentSet = env;
            // m_controls.PreviewModel = Tuple.Create(Context.PreviewModelName, Context.PreviewModelBinding); 
            // m_controls.Object = Context.MaterialName;

            var schemaLoader = Globals.MEFContainer.GetExportedValue<XLELayer.MaterialSchemaLoader>();
            m_controls.Bind(
                new XLELayer.MaterialPropertyContext(
                    // new GUILayer.RawMaterial("game\\chr\\nu_f\\skin\\dragon003.material:nu_f_sk_dragon003"),
                    new GUILayer.RawMaterial(Context.MaterialName),
                    schemaLoader));
        }

        [Import(AllowDefault = false)] private IControlHostService m_controlHostService;
        [Import(AllowDefault = false)] private RenderingInterop.ActiveMaterialContext Context;
        // HierchicalMaterialControl m_controls;

        Sce.Atf.Controls.PropertyEditing.PropertyGrid m_controls;
    }
}
