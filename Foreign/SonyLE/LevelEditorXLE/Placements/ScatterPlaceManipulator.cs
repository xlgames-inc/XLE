using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Drawing;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using Sce.Atf.Controls.PropertyEditing;

using LevelEditorCore;
using Camera = Sce.Atf.Rendering.Camera;

using System.ComponentModel;
using System.Xml.Serialization;

namespace LevelEditorXLE.Placements
{
    [Export(typeof(IManipulator))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class ScatterPlaceManipulator : IManipulator, XLEBridgeUtils.IManipulatorExtra
    {
        public class ManipulatorSettings : IPropertyEditingContext
        {
            public float Radius { get; set; }
            public float Density { get; set; }

            public class Object
            {
                [EditorAttribute(typeof(FileUriEditor), typeof(System.Drawing.Design.UITypeEditor))]
                [TypeConverter(typeof(AssetNameNoExtConverter))]
                public string Model { get; set; }

                [EditorAttribute(typeof(FileUriEditor), typeof(System.Drawing.Design.UITypeEditor))]
                [TypeConverter(typeof(AssetNameNoExtConverter))]
                public string Material { get; set; }

                public string Supplements { get; set; }

                public uint Weight { get; set; }

                public Object()
                {
                    Model = "Game/Model/Nature/BushTree/BushE";
                    Material = "Game/Model/Nature/BushTree/BushE";
                    Supplements = "";
                    Weight = 100;
                }
            };

            public IList<Object> Objects { get { return _objects; } }

            public Object SelectRandomObject(Random rng)
            {
                if (Objects.Count == 0) return new Object();
                uint totalWeight = 0;
                foreach (var o in _objects) totalWeight += o.Weight;

                var r = rng.Next((int)totalWeight);
                for (int c=0; c<_objects.Count; ++c) {
                    if (r < _objects[c].Weight) return _objects[c];
                    r -= (int)_objects[c].Weight;
                }

                return _objects[_objects.Count-1];
            }

            public ManipulatorSettings()
            {
                Radius = 50.0f; Density = 0.1f;
                Objects.Add(new Object());
            }

            private List<Object> _objects = new List<Object>();

            public void SaveModelList(string fn)
            {
                var stream = new System.IO.FileStream(fn, System.IO.FileMode.CreateNew, System.IO.FileAccess.Write);
                var serializer = new XmlSerializer(typeof(List<Object>));
                serializer.Serialize(stream, _objects);
            }

            public void LoadModelList(string fn)
            {
                var stream = new System.IO.FileStream(fn, System.IO.FileMode.Open, System.IO.FileAccess.Read);
                var serializer = new XmlSerializer(typeof(List<Object>));
                var newObjs = serializer.Deserialize(stream) as List<Object>;
                _objects.Clear();
                _objects.InsertRange(0, newObjs);
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
                        // _propertyDescriptors.Add(
                        //     new UnboundPropertyDescriptor(
                        //         GetType(),
                        //         "ModelName", "Model Name", category,
                        //         "Name of the model to create and destroy",
                        //         new Sce.Atf.Controls.PropertyEditing.FileUriEditor(),
                        //         new AssetNameNoExtConverter()));
                        // _propertyDescriptors.Add(
                        //     new UnboundPropertyDescriptor(
                        //         GetType(),
                        //         "MaterialName", "Material Name", category,
                        //         "Material to use with newly created placements",
                        //         new Sce.Atf.Controls.PropertyEditing.FileUriEditor(),
                        //         new AssetNameNoExtConverter()));

                        _propertyDescriptors.Add(
                            new UnboundPropertyDescriptor(
                                GetType(),
                                "Objects", "Models", category,
                                "Collection of models to place",
                                CreateEmbeddedCollectionEditor()));
                    }

                    return _propertyDescriptors;
                }
            }
            List<System.ComponentModel.PropertyDescriptor> _propertyDescriptors;

            private static EmbeddedCollectionEditor CreateEmbeddedCollectionEditor()
            {
                var objectName = "Objects";
                var result = new EmbeddedCollectionEditor();
                result.GetItemInsertersFunc = (context) =>
                {
                    var list = context.GetValue() as IList<Object>;
                    if (list == null) return EmptyArray<EmbeddedCollectionEditor.ItemInserter>.Instance;

                    // create ItemInserter for each component type.
                    var insertors = new EmbeddedCollectionEditor.ItemInserter[1]
                    {
                        new EmbeddedCollectionEditor.ItemInserter(objectName,
                        delegate
                        {
                            var o = new Object();
                            list.Add(o);
                            return o;
                        })
                    };
                    return insertors;
                };

                result.RemoveItemFunc = (context, item) =>
                {
                    var list = context.GetValue() as IList<Object>;
                    if (list != null)
                        list.Remove(item as Object);
                };

                result.MoveItemFunc = (context, item, delta) =>
                {
                    var list = context.GetValue() as IList<Object>;
                    if (list != null)
                    {
                        var node = item as Object;
                        int index = list.IndexOf(node);
                        int insertIndex = index + delta;
                        if (insertIndex < 0 || insertIndex >= list.Count)
                            return;
                        list.RemoveAt(index);
                        list.Insert(insertIndex, node);
                    }
                };

                return result;
            }
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

        public bool ClearBeforeDraw() { return false; }

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

            if (ManipulatorContext.Objects.Count == 0) return;

            GUILayer.EditorInterfaceUtils.ScatterPlaceOperation op;

            var sceneManager = nativeVC.SceneManager;
            using (var editor = sceneManager.GetPlacementsEditor())
            {
                using (var scene = sceneManager.GetIntersectionScene())
                {
                    op = GUILayer.EditorInterfaceUtils.CalculateScatterOperation(
                        editor, scene,
                        ManipulatorContext.Objects.Select(C => C.Model),
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
            var resourceConverter = Globals.MEFContainer.GetExportedValue<ResourceConverterService>();

            foreach (var s in op._creationPositions)
            {
                // select a random entry from the list of objects
                var o = ManipulatorContext.SelectRandomObject(m_rng);

                IResource resource = null;
                foreach (var d in resourceResolvers)
                {
                    resource = d.Resolve(new Uri(
                        new Uri(System.Environment.CurrentDirectory + "\\"),
                        o.Model + "<model"));
                    if (resource != null) break;
                }

                if (resource != null)
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
                        if (p != null)
                        {
                            p.Material = o.Material;
                            p.Supplements = o.Supplements;
                        }
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

