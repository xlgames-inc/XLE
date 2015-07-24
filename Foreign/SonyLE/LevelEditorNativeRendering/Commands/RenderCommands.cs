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
                if (size.Width > size.Height)
                {
                    if (m_splitContainer.Orientation != Orientation.Vertical)
                    {
                        m_splitContainer.Orientation = Orientation.Vertical;
                        m_splitContainer.SplitterDistance = size.Width / 2; 
                    }
                }
                else if (size.Width != 0 && size.Height != 0)
                {
                    if (m_splitContainer.Orientation != Orientation.Horizontal)
                    {
                        m_splitContainer.Orientation = Orientation.Horizontal;
                        m_splitContainer.SplitterDistance = size.Height / 2; 
                    }
                }
            }

            private Sce.Atf.Controls.PropertyEditing.PropertyGrid[] m_propControls;
            private SplitContainer m_splitContainer;
        }

        #region IInitializable Members

        public virtual void Initialize()
        {
            // register a custom menu without any commands, so it won't appear in the main menu bar
            MenuInfo menuInfo = m_commandService.RegisterMenu(this, "RenderModes", "Rendering modes");
            ToolStrip strip = menuInfo.GetToolStrip();
          
             CommandInfo cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderSmooth,
                StandardMenu.View,
                m_commandGroup,
                "Solid".Localize(),
                "Solid rendering".Localize(),
                Keys.None,
                Resources.SmoothImage,
                CommandVisibility.Menu,
                this);
             strip.Items.Add(cmdInfo.GetButton());

             cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderWireFrame,
                StandardMenu.View,
                m_commandGroup,
                "Wireframe".Localize(),
                "Wireframe rendering".Localize(),
                Keys.None,
                Resources.WireframeImage,
                CommandVisibility.Menu,
                this);
             strip.Items.Add(cmdInfo.GetButton());

            cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderOutlined,
                StandardMenu.View,
                m_commandGroup,
                "SolidOverWire",
                "Solid over wireframe rendering".Localize(),
                Keys.None,
                Resources.OutlinedImage,
                CommandVisibility.Menu,
                this);
            strip.Items.Add(cmdInfo.GetButton());

            cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderTextured,
                StandardMenu.View,
                m_commandGroup,
                "Textured".Localize(),
                "Textured rendering".Localize(),
                Keys.T,
                Resources.TexturedImage,
                CommandVisibility.Menu,
                this);
            strip.Items.Add(cmdInfo.GetButton());


            cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderLight,
                StandardMenu.View,
                m_commandGroup,
                "Lighting".Localize(),
                "Lighting".Localize(),
                Keys.L,
                Resources.LightImage,
                CommandVisibility.Menu,
                this);
            strip.Items.Add(cmdInfo.GetButton());

            cmdInfo = m_commandService.RegisterCommand(                
                Command.RenderBackFace,
                StandardMenu.View,
                m_commandGroup,
                "BackFace".Localize(),
                "Render back faces".Localize(),
                Keys.B,
                Resources.BackfaceImage,
                CommandVisibility.Menu,
                this);
            strip.Items.Add(cmdInfo.GetButton());

            cmdInfo = m_commandService.RegisterCommand(
               Command.RenderShadow,
               StandardMenu.View,
               m_commandGroup,
               "Shadow".Localize(),
               "Render shadow".Localize(),
               Keys.None,
               Resources.ShadowImage,
               CommandVisibility.Menu,
               this);
            strip.Items.Add(cmdInfo.GetButton());


            cmdInfo = m_commandService.RegisterCommand(
             Command.RenderNormals,
             StandardMenu.View,
             m_commandGroup,
             "Normals".Localize(),
             "Render Normals".Localize(),
             Keys.None,
             Resources.NormalImage,
             CommandVisibility.Menu,
             this);
            strip.Items.Add(cmdInfo.GetButton());

            m_commandService.RegisterCommand(                
                Command.RenderCycle,
                StandardMenu.View,
                m_commandGroup,
                "CycleRenderModes".Localize(),
                "Cycle render modes".Localize(),
                Keys.Space,
                null,
                CommandVisibility.Menu,
                this);

            //cmdInfo = m_commandService.RegisterCommand(
            //  Command.RealTime,
            //  StandardMenu.View,
            //  m_commandGroup,
            //  "RealTime".Localize(),
            //  "Toggle real time rendering".Localize(),
            //  Keys.None,
            //  Resources.RealTimeImage,
            //  CommandVisibility.Menu,
            //  this);
            //strip.Items.Add(cmdInfo.GetButton());

            m_rsPropertyGrid = new MultiPropControl();
            m_controlHostService.RegisterControl(m_rsPropertyGrid, 
                new ControlInfo("Render settings", "Per viewport render settings", StandardControlGroup.Hidden), null);

            if (m_scriptingService != null)
                m_scriptingService.SetVariable("renderCommands", this);
        }

        #endregion

        #region ICommandClient Members

        /// <summary>
        /// Can the client do the command?</summary>
        /// <param name="commandTag">Command</param>
        /// <returns>true, iff client can do the command</returns>
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            var activeControl = (NativeDesignControl)m_designView.ActiveView;
            var rs = activeControl.RenderState;
            var cam = activeControl.Camera;

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

            switch ((Command)commandTag)
            {
                case Command.RenderSmooth:
                case Command.RenderWireFrame:
                case Command.RenderOutlined:
                case Command.RenderLight:
                case Command.RenderBackFace:
                case Command.RenderShadow:
                case Command.RenderNormals:
                case Command.RenderCycle:
                    //  case Command.RealTime:
                    return true;
                case Command.RenderTextured:
                    return (rs.RenderFlag & GlobalRenderFlags.Solid) != 0;
            }

            return false;
        }

        /// <summary>
        /// Do a command</summary>
        /// <param name="commandTag">Command</param>
        public void DoCommand(object commandTag)
        {
            if (commandTag is Command)
            {
                NativeDesignControl control = (NativeDesignControl)m_designView.ActiveView;
                RenderState rs = control.RenderState;
                
                switch ((Command)commandTag)
                {
                    case Command.RenderSmooth:
                        rs.RenderFlag &= ~(GlobalRenderFlags.WireFrame | GlobalRenderFlags.RenderBackFace);
                        rs.RenderFlag |= (GlobalRenderFlags.Solid | GlobalRenderFlags.Lit | GlobalRenderFlags.Textured);
                        
                        break;

                    case Command.RenderWireFrame:
                        rs.RenderFlag |= (GlobalRenderFlags.WireFrame ); //| RenderFlags.RenderBackFace
                        rs.RenderFlag &= ~(GlobalRenderFlags.Solid | GlobalRenderFlags.Lit | GlobalRenderFlags.Textured);
                        
                        break;

                    case Command.RenderOutlined:
                        rs.RenderFlag |= (GlobalRenderFlags.WireFrame | GlobalRenderFlags.Solid |
                            GlobalRenderFlags.Lit | GlobalRenderFlags.Textured);
                        rs.RenderFlag &= ~GlobalRenderFlags.RenderBackFace;
                        
                        break;

                    case Command.RenderTextured:
                        rs.RenderFlag ^= GlobalRenderFlags.Textured;
                        
                        break;

                    case Command.RenderLight:
                        rs.RenderFlag ^= GlobalRenderFlags.Lit;
                        
                        break;

                    case Command.RenderBackFace:
                        rs.RenderFlag ^= GlobalRenderFlags.RenderBackFace;
                        break;

                    case Command.RenderShadow:
                        rs.RenderFlag ^= GlobalRenderFlags.Shadows;
                        break;

                    case Command.RenderNormals:
                        rs.RenderFlag ^= GlobalRenderFlags.RenderNormals;
                        break;

                    case Command.RenderCycle:
                        GlobalRenderFlags flags = rs.RenderFlag;

                        if ((flags & GlobalRenderFlags.Solid) != 0 &&
                            (flags & GlobalRenderFlags.WireFrame) != 0)
                        {
                            // outlined -> smooth
                            goto case Command.RenderSmooth;
                        }
                        if ((flags & GlobalRenderFlags.Solid) != 0)
                        {
                            // smooth -> wireframe
                            goto case Command.RenderWireFrame;
                        }
                        // wireframe -> outlined
                        goto case Command.RenderOutlined;
                        
                  //  case Command.RealTime:
                  //      m_designView.RealTime = !m_designView.RealTime;
                  //      break;
                }
                m_designView.ActiveView.Invalidate();                
            }
        }

        /// <summary>
        /// Updates command state for given command</summary>
        /// <param name="commandTag">Command</param>
        /// <param name="state">Command state to update</param>
        public void UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state)
        {
            if (commandTag is Command)
            {
                NativeDesignControl control = (NativeDesignControl)m_designView.ActiveView;
                GlobalRenderFlags flags = control.RenderState.RenderFlag;
                switch ((Command)commandTag)
                {
                    case Command.RenderSmooth:
                        state.Check = (flags & GlobalRenderFlags.Solid) != 0;
                        break;

                    case Command.RenderWireFrame:
                        state.Check = (flags & GlobalRenderFlags.WireFrame) != 0;
                        break;

                    case Command.RenderOutlined:
                        state.Check = (flags & GlobalRenderFlags.Solid) != 0 &&
                                      (flags & GlobalRenderFlags.WireFrame) != 0;
                        break;

                    case Command.RenderTextured:
                        state.Check = (flags & GlobalRenderFlags.Textured) != 0;
                        break;

                    case Command.RenderLight:
                        state.Check = ((flags & GlobalRenderFlags.Lit) != 0);
                        break;

                    case Command.RenderBackFace:
                        state.Check = (flags & GlobalRenderFlags.RenderBackFace) != 0;
                        break;
                    case Command.RenderShadow:
                        state.Check = (flags & GlobalRenderFlags.Shadows) == GlobalRenderFlags.Shadows;
                        break;

                    case Command.RenderNormals:
                        state.Check = (flags & GlobalRenderFlags.RenderNormals) == GlobalRenderFlags.RenderNormals;
                        break;
                   // case Command.RealTime:
                  //      state.Check = m_designView.RealTime;                  

                }
            }
        }

        #endregion
        private enum Command
        {
            RenderSmooth,
            RenderWireFrame,
            RenderOutlined,
            RenderTextured,
            RenderLight,
            RenderBackFace,
            RenderShadow,
            RenderNormals,
            RenderCycle,
          //  RealTime
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
