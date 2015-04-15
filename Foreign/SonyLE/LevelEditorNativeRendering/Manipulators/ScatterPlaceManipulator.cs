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

using LevelEditorCore;
using Camera = Sce.Atf.Rendering.Camera;

namespace RenderingInterop
{
    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ScatterPlaceManipulator : Manipulator
    {
        public ScatterPlaceManipulator()
        {
            ManipulatorInfo = new ManipulatorInfo(
                "Scatter placements".Localize(),
                "Scatter placements manipulator".Localize(),
                LevelEditorCore.Resources.CubesImage,
                Keys.None);

            m_helper = new XLELayer.ScatterPlaceManipulatorHelper();
        }

        public override bool Pick(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = m_helper.HitTest(out m_hoverPt, scrPt, vc.ClientSize, vc.Camera);
            return m_hasHoverPt;
        }

        public override void OnBeginDrag() {}
        
        public override void OnDragging(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = m_helper.HitTest(out m_hoverPt, scrPt, vc.ClientSize, vc.Camera);
            if (!m_hasHoverPt) return;

            var game = (vc as NativeDesignControl).DesignView.Context.As<IGame>();
            if (game == null) return;

            var op = m_helper.Perform(m_modelname, m_hoverPt, m_radius, m_density);
            foreach (var d in op._toBeDeleted)
            {
                var adapter = GameEngine.GetAdapterFromId(d.Item1, d.Item2);
                if (adapter != null)
                {
                    adapter.DomNode.RemoveFromParent();
                }
            }

            var resourceResolvers = Globals.MEFContainer.GetExportedValues<IResourceResolver>();
            IResource resource = null;

            foreach (var d in resourceResolvers)
            {
                resource = d.Resolve(new Uri(
                    new Uri(Environment.CurrentDirectory + "\\"),
                    m_modelname + ".dae"));
                if (resource != null) break;
            }

            if (resource != null)
            {
                var resourceConverter = Globals.MEFContainer.GetExportedValue<ResourceConverterService>();
                foreach (var s in op._creationPositions)
                {
                    IGameObject resGob = resourceConverter.Convert(resource);
                    if (resGob != null)
                    {
                        resGob.As<DomNode>().InitializeExtensions();
                        game.AddChild(resGob);
                        resGob.Translation = s;
                        resGob.Rotation = new Sce.Atf.VectorMath.Vec3F(0.0f, 0.0f, (float)(m_rng.NextDouble()) * 2.0f * 3.14159f);
                    }
                }
            }
        }

        public override void OnEndDrag(ViewControl vc, Point scrPt)
        {

        }

        public override void Render(ViewControl vc)
        {
            if (m_hasHoverPt)
            {
                m_helper.Render(vc, m_hoverPt, m_radius);
            }
        }

        protected override Matrix4F GetManipulatorMatrix()
        {
            return null;
        }

        private XLELayer.ScatterPlaceManipulatorHelper m_helper;
        private string m_modelname = "Game/Model/Nature/BushTree/BushE";
        private Vec3F m_hoverPt;
        private bool m_hasHoverPt;
        private float m_radius = 50.0f;
        private float m_density = 0.1f;
        private Random m_rng = new Random();
    }
}

