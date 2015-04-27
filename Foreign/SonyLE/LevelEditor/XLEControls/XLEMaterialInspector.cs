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
            m_controls = new HierchicalMaterialControl();
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
            m_controls.EnvironmentSet = env;
            m_controls.PreviewModel = Tuple.Create(Context.PreviewModelName, Context.PreviewModelBinding); 
            m_controls.Object = Context.MaterialName;
        }

        [Import(AllowDefault = false)] private IControlHostService m_controlHostService;
        [Import(AllowDefault = false)] private RenderingInterop.ActiveMaterialContext Context;
        HierchicalMaterialControl m_controls;
    }
}
