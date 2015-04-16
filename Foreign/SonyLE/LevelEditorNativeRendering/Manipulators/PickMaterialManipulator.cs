using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Drawing;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using Sce.Atf.Controls.PropertyEditing;

using LevelEditorCore;
using Camera = Sce.Atf.Rendering.Camera;

namespace RenderingInterop
{
    [Export(typeof(ActiveMaterialContext))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ActiveMaterialContext
    {
        public string MaterialName { get; set; }
    }

    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class PickMaterialManipulator : Manipulator
    {
        public PickMaterialManipulator()
        {
            ManipulatorInfo = new ManipulatorInfo(
                "Pick Material".Localize(),
                "Eyedropper tool for selecting a material".Localize(),
                LevelEditorCore.Resources.CubesImage,
                Keys.None);
        }

        public override bool Pick(ViewControl vc, Point scrPt) { return true; }
        public override void OnBeginDrag() { }
        public override void OnDragging(ViewControl vc, Point scrPt) {}

        public override void OnEndDrag(ViewControl vc, Point scrPt) 
        {
            var ray = vc.GetWorldRay(scrPt);
            var endPt = ray.Origin + vc.Camera.FarZ * ray.Direction;

            // do an intersection test here, and find the material under the cursor
            var pick = NativeInterop.Picking.RayPick(
                null, ray, vc.Camera, vc.ClientSize, NativeInterop.Picking.Flags.Objects);

            if (pick.Length > 0)
            {
                Context.MaterialName = pick[0].materialName;

                m_highlightDrawIndex = pick[0].drawCallIndex;
                m_highlight.Add(pick[0].documentId, pick[0].instanceId);

                m_highlight.DoFixup(GameEngine.GetEditorSceneManager().GetPlacementsEditor());
            }
            else
            {
                m_highlightDrawIndex = ~0u;
                m_highlight.Clear();
            }
        }

        public override void Render(ViewControl vc)
        {
            // render highlight
            var sceneManager = GameEngine.GetEditorSceneManager();
            using (var context = GameEngine.CreateRenderingContext())
            {
                GUILayer.RenderingUtil.RenderHighlight(
                    context, sceneManager.GetPlacementsEditor(),
                    m_highlight, m_highlightDrawIndex);
            }
        }

        protected override Matrix4F GetManipulatorMatrix()
        {
            return null;
        }

        [Import(AllowDefault = false)] private ActiveMaterialContext Context;
        private GUILayer.ObjectSet m_highlight = new GUILayer.ObjectSet();
        private uint m_highlightDrawIndex;
    }
}

