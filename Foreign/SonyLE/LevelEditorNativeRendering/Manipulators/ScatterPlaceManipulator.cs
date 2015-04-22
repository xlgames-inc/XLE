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
    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ScatterPlaceManipulator : Manipulator
    {
        public class ManipulatorSettings : IPropertyEditingContext
        {
            public float Radius { get; set; }
            public float Density { get; set; }
            public string ModelName { get; set; }

            public ManipulatorSettings()
            {
                Radius = 50.0f; Density = 0.1f;
                ModelName = "Game/Model/Nature/BushTree/BushE";
            }

            #region IPropertyEditingContext items
            IEnumerable<object> IPropertyEditingContext.Items
            {
                get
                {
                    var l = new List<object>();
                    l.Add(this);
                    return l;
                }
            }

            /// <summary>
            /// Gets an enumeration of the property descriptors for the items</summary>
            IEnumerable<System.ComponentModel.PropertyDescriptor> IPropertyEditingContext.PropertyDescriptors
            {
                get
                {
                    if (_propertyDescriptors == null) {
                        var category = "General";
                        _propertyDescriptors = new List<System.ComponentModel.PropertyDescriptor>();
                        _propertyDescriptors.Add(
                            new UnboundPropertyDescriptor(
                                GetType(), 
                                "Radius", "Radius", category, 
                                "Create and destroy objects within this range"));
                        _propertyDescriptors.Add(
                            new UnboundPropertyDescriptor(
                                GetType(),
                                "Density", "Density", category,
                                "Higher numbers mean more objects are created within the same area"));
                        _propertyDescriptors.Add(
                            new UnboundPropertyDescriptor(
                                GetType(),
                                "ModelName", "Model Name", category,
                                "Name of the model to create and destroy",
                                new Sce.Atf.Controls.PropertyEditing.FileUriEditor(),
                                new PropertyEditing.AssetNameNoExtConverter()));
                    }

                    return _propertyDescriptors;
                }
            }
            List<System.ComponentModel.PropertyDescriptor> _propertyDescriptors;
            #endregion
        }

        public ScatterPlaceManipulator()
        {
            ManipulatorContext = new ManipulatorSettings();

            ManipulatorInfo = new ManipulatorInfo(
                "Scatter Placer".Localize(),
                "Scatter Placer manipulator".Localize(),
                LevelEditorCore.Resources.CubesImage,
                Keys.None);
        }

        public override bool Pick(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = HitTest(out m_hoverPt, scrPt, vc);
            return m_hasHoverPt;
        }

        public override void OnBeginDrag() {}
        public override void OnDragging(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = HitTest(out m_hoverPt, scrPt, vc);
            if (!m_hasHoverPt) return;

            var game = (vc as NativeDesignControl).DesignView.Context.As<IGame>();
            if (game == null) return;

            GUILayer.EditorInterfaceUtils.ScatterPlaceOperation op;

            var sceneManager = GameEngine.GetEditorSceneManager();
            using (var editor = sceneManager.GetPlacementsEditor())
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    op = GUILayer.EditorInterfaceUtils.CalculateScatterOperation(
                        editor, scene,
                        ManipulatorContext.ModelName,
                        XLELayer.XLELayerUtils.AsVector3(m_hoverPt),
                        ManipulatorContext.Radius, ManipulatorContext.Density);
                }
            }

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
                    ManipulatorContext.ModelName + ".dae"));
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
                        resGob.Translation = XLELayer.XLELayerUtils.AsVec3F(s);
                        resGob.Rotation = new Sce.Atf.VectorMath.Vec3F(0.0f, 0.0f, (float)(m_rng.NextDouble()) * 2.0f * 3.14159f);
                    }
                }
            }
        }
        public override void OnEndDrag(ViewControl vc, Point scrPt) {}

        public override void Render(ViewControl vc)
        {
            if (m_hasHoverPt)
            {
                using (var context = GameEngine.CreateRenderingContext())
                {
                    GUILayer.RenderingUtil.RenderCylinderHighlight(
                        context, XLELayer.XLELayerUtils.AsVector3(m_hoverPt), ManipulatorContext.Radius);
                }
            }
        }

        protected override Matrix4F GetManipulatorMatrix()
        {
            return null;
        }

        public ManipulatorSettings ManipulatorContext
        {
            get;
            private set;
        }

        private Vec3F m_hoverPt;
        private bool m_hasHoverPt;

        private Random m_rng = new Random();

        private bool HitTest(out Vec3F result, Point pt, ViewControl vc)
        {
            var ray = vc.GetWorldRay(pt);
            var pick = NativeInterop.Picking.RayPick(
                null, ray, vc.Camera, vc.ClientSize, 
                NativeInterop.Picking.Flags.Terrain);

            if (pick.Length > 0)
            {
                result = pick[0].hitPt;
                return true;
            }

            result = new Vec3F(0.0f, 0.0f, 0.0f);
            return false;
        }
    }
}

