//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Controls.PropertyEditing;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.XLEControls
{
    [Export(typeof(XLELayer.IPlacementControls))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLEPlacementControls : XLELayer.IPlacementControls, IInitializable
    {
        void IInitializable.Initialize()
        {
            Controls = new XLEManipControls();
            m_controlHostService.RegisterControl(
                Controls, 
                new ControlInfo("Placements Manipulator", "Placement manipulator properties", StandardControlGroup.Right), 
                null);
            Controls.SetActiveContext(m_activeContext);
        }

        public XLEManipControls Controls { get; private set; }

        public XLELayer.ActiveManipulatorContext ActiveContext
        {
            set 
            {
                m_activeContext = value;
                if (Controls != null)
                {
                    Controls.SetActiveContext(value);
                }
            }
        }

        [Import(AllowDefault = false)]
        IControlHostService m_controlHostService;
        XLELayer.ActiveManipulatorContext m_activeContext;
    }
}
