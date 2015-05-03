//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System;
using System.ComponentModel.Composition;
using System.Drawing;
using System.Windows.Forms;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.VectorMath;

using Camera = Sce.Atf.Rendering.Camera;
using ViewTypes = Sce.Atf.Rendering.ViewTypes;

using LevelEditorCore;

namespace LevelEditor.XLEControls
{

    [Export(typeof(CameraController))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class XLECamera : CameraController
    {
        public XLECamera()
        {
            CommandInfo = new CommandInfo(
                   this,
                   StandardMenu.View,
                   StandardCommandGroup.ViewCamera,
                   "camera".Localize() + "/" + "XLE".Localize(),
                   "XLE orbit and plan camera".Localize(),
                   Sce.Atf.Input.Keys.None,
                   LevelEditorCore.Resources.MayaImage,
                   CommandVisibility.Menu);
        }

        public override bool MouseDown(object sender, MouseEventArgs e)
        {
            if (InputScheme.ActiveControlScheme.IsControllingCamera(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)))
            {
                m_lastMousePoint = e.Location;
                m_dragging = true;
                return true;
            }

            if (Control.ModifierKeys.HasFlag(Keys.Control) && e.Button == MouseButtons.Left)
            {
                // "control + l click" repositions the "focus" point of the camera
                // this is critical when we want to be able to rotate around a specific point in the world
                ViewControl c = sender as ViewControl;
                if (c != null) {
                    var hit = RenderingInterop.NativeInterop.Picking.RayPick(
                        null, c.GetWorldRay(e.Location), Camera, c.ClientSize,
                        RenderingInterop.NativeInterop.Picking.Flags.AllWorldObjects);
                    if (hit.Length > 0)
                    {
                        Vec3F transformedPt;
                        Camera.AxisSystem.Transform(hit[0].hitPt, out transformedPt);
                        Camera.Set(Camera.Eye, transformedPt, Camera.Up);
                    }
                }
                return true;
            }

            return base.MouseDown(sender, e);
        }

        private static Vec3F CartesianToSphericalYUp(Vec3F cartesian) 
        {
            float length = cartesian.Length;
            return new Vec3F(
                (float)Math.Acos(cartesian.Y / length),
                (float)Math.Atan2(cartesian.Z, cartesian.X),
                length);
        }

        private static Vec3F SphericalToCartesianYUp(Vec3F spherical) 
        {
            return new Vec3F(
                (float)(spherical[2] * Math.Sin(spherical[0]) * Math.Cos(spherical[1])),
                (float)(spherical[2] * Math.Cos(spherical[0])),
                (float)(spherical[2] * Math.Sin(spherical[0]) * Math.Sin(spherical[1])));
        }

        private static float Clamp(float value, float min, float max) 
        {
            return Math.Max(Math.Min(value, max), min);
        }

        public override bool MouseMove(object sender, MouseEventArgs e)
        {
            if (m_dragging &&
                InputScheme.ActiveControlScheme.IsControllingCamera(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)))
            {
                Control c = sender as Control;
                float dx = e.X - m_lastMousePoint.X;
                float dy = e.Y - m_lastMousePoint.Y;

                if (InputScheme.ActiveControlScheme.IsRotating(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)) &&
                    (Camera.ViewType == ViewTypes.Perspective || LockOrthographic == false))
                {
                    var orbitRotationSpeed = .0005f * (float)Math.PI;

                        // just do this orbitting calculation in spherical coords...
                        // it's much easier than any other method
                    var orbitCenter = Camera.LookAtPoint;
                    var spherical = CartesianToSphericalYUp(orbitCenter - Camera.Eye);
                    spherical[0] += dy * orbitRotationSpeed;
                    spherical[0] = Clamp(spherical[0], (float)Math.PI * 0.02f, (float)Math.PI * 0.98f);
                    spherical[1] += dx * orbitRotationSpeed;

                    var defaultUp = new Vec3F(0.0f, 1.0f, 0.0f);    // (geometry gets hopelessly unnormalized if we don't reset default-up here)
                    Camera.Set(orbitCenter - SphericalToCartesianYUp(spherical), orbitCenter, defaultUp);
                    if (Camera.ViewType != ViewTypes.Perspective)
                        Camera.ViewType = ViewTypes.Perspective;
                }
                else if (InputScheme.ActiveControlScheme.IsZooming(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)))
                {
                    float zoom = (-dy - dx);

                    float adj = Camera.DistanceFromLookAt;
                    var zoomSpeed = 0.01f * adj;
                    var lookAtDir = Vec3F.Normalize(Camera.LookAt);
                    var movement = Math.Min(zoom * zoomSpeed, Camera.DistanceFromLookAt - 0.1f);
                    Camera.Set(Camera.Eye + lookAtDir * movement, Camera.LookAtPoint, Camera.Up);
                }
                else if (InputScheme.ActiveControlScheme.IsPanning(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)))
                {
                    float adj = Camera.DistanceFromLookAt;
                    var panSpeed = 0.001f * adj;
                    var lookAtPoint = Camera.LookAtPoint;
                    var translation = (Camera.Up * dy * panSpeed) + (Camera.Right * -dx * panSpeed);
                    Camera.Set(Camera.Eye + translation, lookAtPoint + translation, Camera.Up);
                }

                m_lastMousePoint = e.Location;
                return true;
            }

            return base.MouseMove(sender, e);
        }

        public override bool MouseUp(object sender, MouseEventArgs e)
        {
            if (m_dragging)
            {
                m_dragging = false;
                return true;
            }
            return base.MouseUp(sender, e);
        }

        public override bool MouseWheel(object sender, MouseEventArgs e)
        {
            if (!InputScheme.ActiveControlScheme.IsZooming(KeysInterop.ToAtf(Control.ModifierKeys), MouseEventArgsInterop.ToAtf(e)))
                return true;

            float adj = Camera.DistanceFromLookAt;
            var zoomSpeed = .1f / 120.0f * adj;
            var lookAtDir = Vec3F.Normalize(Camera.LookAt);
            var movement = Math.Min(e.Delta * zoomSpeed, Camera.DistanceFromLookAt - 0.1f);
            Camera.Set(Camera.Eye + lookAtDir * movement, Camera.LookAtPoint, Camera.Up);
            return true;
        }

        protected override void CameraToController(Camera camera)
        {
            base.CameraToController(camera);
        }

        protected override void ControllerToCamera(Camera camera)
        {
            base.ControllerToCamera(camera);
        }

        private Point m_lastMousePoint;
        private bool m_dragging;
    }

}
