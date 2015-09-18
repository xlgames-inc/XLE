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

namespace LevelEditorXLE.Placements
{
    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ScatterPlaceManipulator : IManipulator
    {
        public class ManipulatorSettings : IPropertyEditingContext
        {
            public float Radius { get; set; }
            public float Density { get; set; }
            public string ModelName { get; set; }
            public string MaterialName { get; set; }

            public ManipulatorSettings()
            {
                Radius = 50.0f; Density = 0.1f;
                ModelName = "Game/Model/Nature/BushTree/BushE";
                MaterialName = "Game/Model/Nature/BushTree/BushE";
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
                                new AssetNameNoExtConverter()));
                        _propertyDescriptors.Add(
                            new UnboundPropertyDescriptor(
                                GetType(),
                                "MaterialName", "Material Name", category,
                                "Material to use with newly created placements",
                                new Sce.Atf.Controls.PropertyEditing.FileUriEditor(),
                                new AssetNameNoExtConverter()));
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
                Resources.ScatterPlace,
                Keys.None);
        }

        public bool Pick(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = HitTest(out m_hoverPt, scrPt, vc);
            return m_hasHoverPt;
        }

        public void OnBeginDrag() {}
        public void OnDragging(ViewControl vc, Point scrPt)
        {
            m_hasHoverPt = HitTest(out m_hoverPt, scrPt, vc);
            if (!m_hasHoverPt) return;

            var nativeVC = vc as XLEBridgeUtils.NativeDesignControl;
            if (nativeVC == null) return;

            var game = nativeVC.DesignView.Context.As<IGame>();
            if (game == null) return;

            GUILayer.EditorInterfaceUtils.ScatterPlaceOperation op;

            var sceneManager = nativeVC.SceneManager;
            using (var editor = sceneManager.GetPlacementsEditor())
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    op = GUILayer.EditorInterfaceUtils.CalculateScatterOperation(
                        editor, scene,
                        ManipulatorContext.ModelName,
                        XLEBridgeUtils.Utils.AsVector3(m_hoverPt),
                        ManipulatorContext.Radius, ManipulatorContext.Density);
                }
            }

            foreach (var d in op._toBeDeleted)
            {
                var adapter = m_nativeIdMapping.GetAdapter(d.Item1, d.Item2).As<DomNodeAdapter>();
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
                    new Uri(System.Environment.CurrentDirectory + "\\"),
                    ManipulatorContext.ModelName + "<model"));
                if (resource != null) break;
            }

            if (resource != null)
            {
                var resourceConverter = Globals.MEFContainer.GetExportedValue<ResourceConverterService>();
                foreach (var s in op._creationPositions)
                {
                    var resGob = resourceConverter.Convert(resource);
                    if (resGob != null)
                    {
                        resGob.As<DomNode>().InitializeExtensions();

                        var hierarchical = game.AsAll<IHierarchical>();
                        foreach (var h in hierarchical)
                            if (h.AddChild(resGob)) break;

                        var transform = resGob.As<LevelEditorCore.ITransformable>();
                        transform.Translation = XLEBridgeUtils.Utils.AsVec3F(s);
                        transform.Rotation = new Sce.Atf.VectorMath.Vec3F(0.0f, 0.0f, (float)(m_rng.NextDouble()) * 2.0f * 3.14159f);

                            // set the material name (if we can)
                        var p = resGob.As<Placements.XLEPlacementObject>();
                        if (p!=null)
                            p.Material = ManipulatorContext.MaterialName;
                    }
                }
            }
        }
        public void OnEndDrag(ViewControl vc, Point scrPt) {}
        public void OnMouseWheel(LevelEditorCore.ViewControl vc, Point scrPt, int delta) { }

        public void Render(ViewControl vc)
        {
            if (m_hasHoverPt)
            {
                using (var context = XLEBridgeUtils.NativeDesignControl.CreateSimpleRenderingContext(null))
                {
                    GUILayer.RenderingUtil.RenderCylinderHighlight(
                        context, XLEBridgeUtils.Utils.AsVector3(m_hoverPt), ManipulatorContext.Radius);
                }
            }
        }

        public ManipulatorSettings ManipulatorContext
        {
            get;
            private set;
        }

        public ManipulatorInfo ManipulatorInfo
        {
            get;
            protected set;
        }

        private Vec3F m_hoverPt;
        private bool m_hasHoverPt;

        private Random m_rng = new Random();

        private bool HitTest(out Vec3F result, Point pt, ViewControl vc)
        {
            var ray = vc.GetWorldRay(pt);
            var pick = XLEBridgeUtils.Picking.RayPick(
                vc, ray, XLEBridgeUtils.Picking.Flags.Terrain);

            if (pick != null && pick.Length > 0)
            {
                result = pick[0].hitPt;
                return true;
            }

            result = new Vec3F(0.0f, 0.0f, 0.0f);
            return false;
        }

        [Import(AllowDefault = false)]
        private INativeIdMapping m_nativeIdMapping;
    }
}

