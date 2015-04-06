//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.TerrainControls
{
    [Export(typeof(XLELayer.ITerrainControls))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLETerrainControls : XLELayer.ITerrainControls, IInitializable
    {
        void IInitializable.Initialize()
        {
            Controls = new XLETerrainManipControls();
            m_controlHostService.RegisterControl(
                Controls, 
                new ControlInfo("Terrain Manipulator", "Terrain manipulator properties", StandardControlGroup.Right), 
                null);
        }

        public XLETerrainManipControls Controls { get; private set; }
        public string ActiveManipulator { get { return Controls.ActiveManipulator; } }
        public GUILayer.IManipulatorSet Manipulators
        {
            set 
            {
                m_manipulators = value; 
                Controls.SetManipulators(value);
            }
            get { return m_manipulators; }
        }

        private GUILayer.IManipulatorSet m_manipulators;

        [Import(AllowDefault = false)]
        IControlHostService m_controlHostService;
    }
}
