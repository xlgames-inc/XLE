//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.VectorMath;

using LevelEditorCore;
using Sce.Atf.Controls.PropertyEditing;
using Resources = LevelEditorCore.Resources;
using PropertyDescriptor = System.ComponentModel.PropertyDescriptor;

namespace RenderingInterop
{
    using Camera = Sce.Atf.Rendering.Camera;

    /// <summary>
    /// Commands for switching the active DesignControl's rendering mode</summary>
    [Export(typeof(IInitializable))]
    [Export(typeof(RenderCommands))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class RenderCommands : ICommandClient, IInitializable
    {
        public class MultiPropControl : UserControl
        {
            public MultiPropControl()
            {
                m_propControls = new Sce.Atf.Controls.PropertyEditing.PropertyGrid[2];
                m_propControls[0] = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
                m_propControls[1] = new Sce.Atf.Controls.PropertyEditing.PropertyGrid();
                Layout += OnLayout;

                m_splitContainer = new SplitContainer();

                SuspendLayout();
                m_splitContainer.SuspendLayout();
                m_splitContainer.Panel1.SuspendLayout();
                m_splitContainer.Panel2.SuspendLayout();

                m_propControls[0].Dock = DockStyle.Fill;
                m_splitContainer.Panel1.Controls.Add(m_propControls[0]);

                m_propControls[1].Dock = DockStyle.Fill;
                m_splitContainer.Panel2.Controls.Add(m_propControls[1]);

                m_splitContainer.FixedPanel = FixedPanel.None;
                m_splitContainer.Dock = DockStyle.Fill;
                Controls.Add(m_splitContainer);

                m_splitContainer.Panel2.ResumeLayout(false);
                m_splitContainer.Panel1.ResumeLayout(false);
                m_splitContainer.ResumeLayout(false);
                this.ResumeLayout(true);
            }

            public Sce.Atf.Controls.PropertyEditing.PropertyGrid GetGrid(int index) { return m_propControls[index]; }

            private void OnLayout(object sender, LayoutEventArgs e) 
            {
                    // flip the orientation of the splitter based on the size of the window
                var size = ClientSize;
                m_splitContainer.SuspendLayout();
                if (size.Width > size.Height)
                {
                    if (m_splitContainer.Width > 0 && m_splitContainer.Height > 0
                        && m_splitContainer.Orientation != Orientation.Vertical)
                    {
                        m_splitContainer.Orientation = Orientation.Vertical;
                        m_splitContainer.SplitterDistance = size.Width / 2; 
                    }
                }
                else if (size.Width != 0 && size.Height != 0)
                {
                    if (m_splitContainer.Width > 0 && m_splitContainer.Height > 0
                        && m_splitContainer.Orientation != Orientation.Horizontal)
                    {
                        m_splitContainer.Orientation = Orientation.Horizontal;
                        m_splitContainer.SplitterDistance = size.Height / 2; 
                    }
                }
                m_splitContainer.ResumeLayout();
            }

            private Sce.Atf.Controls.PropertyEditing.PropertyGrid[] m_propControls;
            private SplitContainer m_splitContainer;
        }

        #region IInitializable Members

        public virtual void Initialize()
        {
            m_commandService.RegisterCommand(
                Command.Screenshot, StandardMenu.View,
                m_commandGroup, "Screenshot".Localize(), "Take screenshot".Localize(),
                Keys.None, null, CommandVisibility.Menu,
                this);

            m_rsPropertyGrid = new MultiPropControl();
            m_controlHostService.RegisterControl(
                m_rsPropertyGrid, 
                new ControlInfo("Render settings", "Per viewport render settings", StandardControlGroup.Hidden), null);

            if (m_scriptingService != null)
                m_scriptingService.SetVariable("renderCommands", this);
        }

        #endregion

        void BindToControl(NativeDesignControl ctrl)
        {
            var rs = ctrl.RenderState;
            var cam = ctrl.Camera;

            {
                var context = m_rsPropertyGrid.GetGrid(0).PropertyGridView.EditingContext as RenderStateEditingContext;
                if (context == null || context.Item != rs)
                {
                    context = new RenderStateEditingContext(rs);
                    m_rsPropertyGrid.GetGrid(0).Bind(context);
                }
            }

            {
                var context = m_rsPropertyGrid.GetGrid(1).PropertyGridView.EditingContext as CameraEditingContext;
                if (context == null || context.Item != cam)
                {
                    context = new CameraEditingContext(cam);
                    m_rsPropertyGrid.GetGrid(1).Bind(context);
                }
            }
        }

        #region ICommandClient Members
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            var activeControl = (NativeDesignControl)m_designView.ActiveView;

                // Convenient to bind here, because we can't otherwise track when the active window changes.
                // But it means that we must have at least one command, or we won't get events here!
            BindToControl(activeControl);

            switch ((Command)commandTag)
            {
                case Command.Screenshot:
                    return true;
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;
            switch ((Command)commandTag)
            {
                case Command.Screenshot:
                    System.Windows.Forms.MessageBox.Show("Screenshots from editor not yet supported");
                    break;
            }
        }

        public void UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state)
        {}

        #endregion

        private enum Command
        {
            Screenshot
        }

        private MultiPropControl m_rsPropertyGrid;
              
        [Import(AllowDefault = false)]
        private ICommandService m_commandService;

        [Import(AllowDefault = false)]
        private IControlHostService m_controlHostService;

        [Import(AllowDefault = false)]
        private IDesignView m_designView = null;
        
        [Import(AllowDefault = true)]
        private ScriptingService m_scriptingService;

        private string m_commandGroup = "RenderingModes";

        private class RenderStateEditingContext : IPropertyEditingContext
        {
            public IEnumerable<PropertyDescriptor> PropertyDescriptors
            {
                get 
                {
                    if (s_propertyDescriptor == null)
                    {                        
                        var colorEd = new Sce.Atf.Controls.ColorPickerEditor();
                        string category = "Render Settings".Localize();                        
                        
                        s_propertyDescriptor = new PropertyDescriptor[]
                        {
                            new UnboundPropertyDescriptor(typeof(RenderState),"WireFrameColor","Wire FrameColor".Localize(),category,"color used for wireframe mode".Localize(),colorEd),
                            new UnboundPropertyDescriptor(typeof(RenderState),"SelectionColor","Selection Color".Localize(),category,"Wireframe color for selected objects".Localize(),colorEd),
                            new UnboundPropertyDescriptor(typeof(RenderState),"DisplayCaption","Display Caption".Localize(),category,"Display object name".Localize()),
                            new UnboundPropertyDescriptor(typeof(RenderState),"DisplayBound","Display Bound".Localize(),category,"Display objects' bounding volume".Localize()),
                            new UnboundPropertyDescriptor(typeof(RenderState),"DisplayPivot","Display Pivot".Localize(),category,"Display object pivot".Localize()),
                            new UnboundPropertyDescriptor(typeof(RenderState),"EnvironmentSettings","Environment Settings".Localize(),category,"Environment settings config to use".Localize()),
                            new UnboundPropertyDescriptor(typeof(RenderState),"GridMode","Grid mode".Localize(),category,"Grid rendering mode".Localize())
                        };
                    }
                    return s_propertyDescriptor;
                }
            }

            public RenderStateEditingContext(RenderState rs) { m_items[0] = rs; }
            public object Item { get { return m_items[0];} }
            public IEnumerable<object> Items { get { return m_items; } }

            private readonly object[] m_items = new object[1];
            private static PropertyDescriptor[] s_propertyDescriptor;
        }

        private class CameraEditingContext : IPropertyEditingContext
        {
            private class Helper
            {
                public Vec3F WorldEye           { get { return m_cam.WorldEye; } }
                public Vec3F WorldLookAtPoint   { get { return m_cam.WorldLookAtPoint; } }
                public Vec3F WorldRight         { get { return m_cam.WorldRight; } }
                public Vec3F WorldUp            { get { return m_cam.WorldUp; } }

                public float YFov { 
                    get { 
                        return m_cam.YFov * 180.0f / MathHelper.Pi;
                    }
                    set {
                        if (m_cam.ProjectionType == Sce.Atf.Rendering.ProjectionType.Perspective) {
                            m_cam.SetPerspective(value * MathHelper.Pi / 180.0f, m_cam.Aspect, m_cam.NearZ, m_cam.FarZ);
                        }
                    }
                }
                public float NearZ      { get { return m_cam.NearZ; } }
                public float FarZ       { get { return m_cam.FarZ; } set { m_cam.FarZ = value; } }

                public Camera BoundCamera { get { return m_cam; } }

                public Helper(Camera cam) { m_cam = cam; }
                private Camera m_cam;
            }

            public IEnumerable<PropertyDescriptor> PropertyDescriptors
            {
                get
                {
                    if (s_propertyDescriptor == null)
                    {
                        string category0 = "Position".Localize();
                        string category1 = "Parameters".Localize();

                        s_propertyDescriptor = new PropertyDescriptor[]
                        {
                            new UnboundPropertyDescriptor(typeof(Helper), "WorldEye",           "Eye".Localize(),               category0, "Position of the camera".Localize()),
                            new UnboundPropertyDescriptor(typeof(Helper), "WorldLookAtPoint",   "Look At Point".Localize(),     category0, "Focus point".Localize()),
                            new UnboundPropertyDescriptor(typeof(Helper), "WorldRight",         "Right Vector".Localize(),      category0, "Direction to the right of the screen".Localize()),
                            new UnboundPropertyDescriptor(typeof(Helper), "WorldUp",            "Up Vector".Localize(),         category0, "Direction to the top of the screen".Localize()),

                            new UnboundPropertyDescriptor(typeof(Helper), "YFov",               "Field of View".Localize(),     category1, "Vertical field of view (degrees)".Localize(),
                                new BoundedFloatEditor(1, 79)),
                            new UnboundPropertyDescriptor(typeof(Helper), "NearZ",              "Near Clip Plane".Localize(),   category1, "Near clip plane distance".Localize()),
                            new UnboundPropertyDescriptor(typeof(Helper), "FarZ",               "Far Clip Plane".Localize(),    category1, "Far clip plane distance".Localize())
                        };
                    }
                    return s_propertyDescriptor;
                }
            }

            public CameraEditingContext(Sce.Atf.Rendering.Camera cam) { m_items[0] = new Helper(cam); }
            public object Item { get { return (m_items[0] as Helper).BoundCamera; } }
            public IEnumerable<object> Items { get { return m_items; } }

            private readonly object[] m_items = new object[1];
            private static PropertyDescriptor[] s_propertyDescriptor;
        }
    }
}
