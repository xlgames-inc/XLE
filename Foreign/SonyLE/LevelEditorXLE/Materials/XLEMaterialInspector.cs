// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

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
using System.Reflection;

namespace LevelEditorXLE.Materials
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class XLEMaterialInspector : IInitializable
    {
        void IInitializable.Initialize()
        {
            m_controls = new HierchicalMaterialControl();
            m_controls.FocusedMatControls.AddExtraControls("Material", new MatTab());
            Context.OnChange += OnActiveMaterialChange;
            m_controlHostService.RegisterControl(
                m_controls,
                new ControlInfo("Material inspector", "Properties for the picked material", StandardControlGroup.Right),
                null);
        }

        void OnActiveMaterialChange()
        {
            // RenderingInterop.GameEngine.GetEditorSceneManager()
            var sceneManager = XLEBridgeUtils.NativeManipulatorLayer.SceneManager;
            var env = new GUILayer.EnvironmentSettingsSet(sceneManager);
            env.AddDefault();
            m_controls.EnvironmentSet = env;
            m_controls.PreviewModel = Tuple.Create(Context.PreviewModelName, Context.PreviewModelBinding); 
            m_controls.Object = Context.MaterialName;
        }

        [Import(AllowDefault = false)] private IControlHostService m_controlHostService;
        [Import(AllowDefault = false)] private ActiveMaterialContext Context;
        HierchicalMaterialControl m_controls;
    }

    class MatTab : ControlsLibrary.MaterialEditor.MaterialControl.ExtraControls
    {
        public MatTab()
        {
            SuspendLayout();
            m_child = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();

            m_child.Location = new System.Drawing.Point(158, 3);
            m_child.Size = new System.Drawing.Size(341, 188);
            m_child.TabIndex = 1;
            m_child.Dock = DockStyle.Fill;
            m_child.Visible = true;

            Controls.Add(m_child);
            Dock = DockStyle.Fill;
            Padding = new System.Windows.Forms.Padding(0);
            ResumeLayout(false);
        }

        public override GUILayer.RawMaterial Object
        {
            set
            {
                if (value != null)
                {
                    m_child.Bind(m_materialSchemaLoader.CreatePropertyContext(value));
                }
                else
                {
                    m_child.Bind(null);
                }
            }
        }

        private Sce.Atf.Controls.PropertyEditing.PropertyGrid m_child;
        [Import(AllowDefault = false)]
        private MaterialSchemaLoader m_materialSchemaLoader;
    }

    [Export(typeof(MaterialSchemaLoader))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class MaterialSchemaLoader : XLEBridgeUtils.DataDrivenPropertyContextHelper
    {
        public IPropertyEditingContext CreatePropertyContext(GUILayer.RawMaterial material)
        {
            var ps = new GUILayer.BasicPropertySource(
                new XLEBridgeUtils.RawMaterialShaderConstants_GetAndSet(material),
                GetPropertyDescriptors("gap:RawMaterial"));
            return new XLEBridgeUtils.PropertyBridge(ps);
        }

        public MaterialSchemaLoader()
        {
            SchemaResolver = new ResourceStreamResolver(Assembly.GetExecutingAssembly(), ".");
            Load("material.xsd");
        }
    };
}
